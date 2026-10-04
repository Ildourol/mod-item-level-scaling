/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingLive.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingIdentity.h"
#include "ItemScalingLootScript.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingSafety.h"
#include "ItemScalingSnapshot.h"
#include "AllCreatureScript.h"
#include "AllGameObjectScript.h"
#include "AllMapScript.h"
#include "AsyncCallbackProcessor.h"
#include "Chat.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "GameObject.h"
#include "Group.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ServerScript.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "StringFormat.h"
#include "Transaction.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include <algorithm>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace
{
    char const* const SlotTable = "mod_item_level_scaling_slot";
    char const* const SnapshotTable = "mod_item_level_scaling_staged_item";
    char const* const MappingTable = "mod_item_level_scaling_staged_variant";
    char const* const PlaceholderName = "ItemScaling reserved";

    struct SourceKey
    {
        uint32 map;
        uint32 instance;
        ObjectGuid guid;
        bool operator<(SourceKey const& other) const
        {
            return std::tie(map, instance, guid) < std::tie(other.map, other.instance, other.guid);
        }
    };

    SourceKey Source(Player const& owner, ObjectGuid guid)
    {
        return {owner.GetMapId(), owner.GetInstanceId(), guid};
    }

    void StartChestRolls(Player* player, GameObject* go)
    {
        if (Group* group = player->GetGroup())
            if (go->GetGOInfo()->chest.groupLootRules)
            {
                switch (group->GetLootMethod())
                {
                    case GROUP_LOOT: group->GroupLoot(&go->loot, go); break;
                    case NEED_BEFORE_GREED: group->NeedBeforeGreed(&go->loot, go); break;
                    case MASTER_LOOT: group->MasterLoot(&go->loot, go); break;
                    default: break;
                }
            }
        go->SetLootState(GO_ACTIVATED, player);
    }

    std::string StagedMapping(ItemTemplate const& item, ItemTemplate const& base, VariantKey const& key)
    {
        auto identity = ItemScalingIdentity::Capture(base);
        return Acore::StringFormat(
            "INSERT INTO {} (variant_entry,base_entry,target_effective_level,target_item_level,formula_version,"
            "generator_revision,required_level,random_property_id,base_class,base_subclass,"
            "base_sound_override_subclass,base_material,base_displayid,base_inventory_type,base_sheath,"
            "preserve_nonzero_stats) VALUES ({},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{})",
            MappingTable, item.ItemId, key.baseEntry, uint32(key.targetEffectiveLevel), key.targetItemLevel,
            uint32(key.formulaVersion), uint32(key.generatorRevision), uint32(key.requiredLevel), key.randomPropertyId,
            identity.itemClass, identity.subClass, identity.soundOverrideSubclass, identity.material, identity.displayId,
            identity.inventoryType, identity.sheath, uint32(sItemScalingConfig->PreserveNonZeroStats));
    }
}

struct ItemScalingLive::Impl
{
    enum class State { Pending, Durable, Ready, Failed };
    struct Request
    {
        ItemTemplate base;
        ItemTemplate scaled{};
        uint8 playerLevel;
        State state{State::Pending};
    };
    struct PendingItem
    {
        std::size_t index;
        uint32 original;
        uint32 count;
        int32 property;
        uint32 suffix;
        VariantKey key;
    };
    struct PendingLoot
    {
        uint64 generation;
        uint64 started;
        ObjectGuid owner;
        std::vector<PendingItem> items;
        std::map<ObjectGuid, LootType> openers;
        bool preparedChest{false};
    };
    struct CatalogueSource
    {
        uint32 entry;
        bool chest;
        std::vector<uint32> items;
    };
    struct Visit
    {
        uint8 level{0};
        uint32 revision{0};
        std::size_t sourceIndex{0};
        std::size_t itemIndex{0};
        std::size_t rollIndex{0};
        uint8 announcedLevel{0};
    };

    mutable std::mutex mutex;
    bool enabled{false};
    uint64 now{0};
    uint64 generation{0};
    uint64 failures{0};
    std::size_t pendingCount{0};
    std::deque<uint32> slots;
    std::unordered_set<uint32> owned;
    std::unordered_set<uint32> reserved;
    std::unordered_map<VariantKey, Request, VariantKeyHash> requests;
    std::deque<VariantKey> work;
    std::deque<VariantKey> durable;
    std::map<SourceKey, PendingLoot> sources;
    std::map<std::pair<uint32, uint32>, Visit> visits;
    std::map<std::pair<ObjectGuid, uint32>, uint64> queries;
    std::unordered_map<uint32, std::vector<CatalogueSource>> catalogue;
    std::unordered_map<uint32, std::vector<int32>> enchantments;
    AsyncCallbackProcessor<TransactionCallback> callbacks;

    bool LoadCatalogue();
    void Prewarm(uint32 budget, std::map<std::pair<uint32, uint32>, Visit>& activeVisits);
};

ItemScalingLive::ItemScalingLive() : _impl(std::make_unique<Impl>()) { }
ItemScalingLive::~ItemScalingLive() = default;

ItemScalingLive* ItemScalingLive::instance()
{
    static ItemScalingLive instance;
    return &instance;
}

bool ItemScalingLive::RecoverStagedTemplates()
{
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS mod_item_level_scaling_slot (entry INT UNSIGNED NOT NULL,"
        "assigned TINYINT UNSIGNED NOT NULL DEFAULT 0, PRIMARY KEY(entry)) "
        "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
    WorldDatabase.DirectExecute("CREATE TABLE IF NOT EXISTS mod_item_level_scaling_staged_item LIKE item_template");
    WorldDatabase.DirectExecute("CREATE TABLE IF NOT EXISTS mod_item_level_scaling_staged_variant LIKE scaled_item_variant");

    QueryResult engines = WorldDatabase.Query(
        "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND ENGINE='InnoDB' "
        "AND TABLE_NAME IN ('item_template','scaled_item_variant','mod_item_level_scaling_slot',"
        "'mod_item_level_scaling_staged_item','mod_item_level_scaling_staged_variant')");
    if (!engines || engines->Fetch()[0].Get<uint32>() != 5)
        return false;
    // SELECT * promotion requires identical column order, types and nullability. Never
    // attempt recovery with a staging table left behind by an incompatible schema change.
    QueryResult columns = WorldDatabase.Query(
        "SELECT TABLE_NAME,COLUMN_NAME,COLUMN_TYPE,IS_NULLABLE FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('item_template','scaled_item_variant',"
        "'mod_item_level_scaling_staged_item','mod_item_level_scaling_staged_variant') "
        "ORDER BY TABLE_NAME,ORDINAL_POSITION");
    if (!columns)
        return false;
    std::map<std::string, std::vector<std::tuple<std::string, std::string, std::string>>> schemas;
    do
    {
        Field* column = columns->Fetch();
        schemas[column[0].Get<std::string>()].emplace_back(column[1].Get<std::string>(),
            column[2].Get<std::string>(), column[3].Get<std::string>());
    } while (columns->NextRow());
    if (schemas["item_template"] != schemas[SnapshotTable] || schemas["scaled_item_variant"] != schemas[MappingTable])
        return false;

    // Verify complete ownership and payload before removing any placeholder. A failed recovery
    // must stop startup: previously awarded items may still depend on these snapshots.
    QueryResult counts = WorldDatabase.Query(
        "SELECT (SELECT COUNT(*) FROM mod_item_level_scaling_staged_item),"
        "(SELECT COUNT(*) FROM mod_item_level_scaling_staged_variant),"
        "(SELECT COUNT(*) FROM mod_item_level_scaling_staged_item s "
        "JOIN mod_item_level_scaling_staged_variant v ON v.variant_entry=s.entry "
        "JOIN mod_item_level_scaling_slot p ON p.entry=s.entry AND p.assigned=1 "
        "JOIN item_template i ON i.entry=s.entry AND i.name='ItemScaling reserved' "
        "WHERE s.ItemLevel=v.target_item_level AND s.RequiredLevel=v.required_level "
        "AND s.class=v.base_class AND s.subclass=v.base_subclass "
        "AND s.SoundOverrideSubclass=v.base_sound_override_subclass AND s.Material=v.base_material "
        "AND s.displayid=v.base_displayid AND s.InventoryType=v.base_inventory_type AND s.sheath=v.base_sheath)");
    if (!counts)
        return false;
    Field* fields = counts->Fetch();
    uint64 staged = fields[0].Get<uint64>();
    if (staged != fields[1].Get<uint64>() || staged != fields[2].Get<uint64>())
        return false;
    if (staged)
    {
        auto transaction = WorldDatabase.BeginTransaction();
        transaction->Append(
            "DELETE i FROM item_template i JOIN mod_item_level_scaling_slot p ON p.entry=i.entry "
            "JOIN mod_item_level_scaling_staged_item s ON s.entry=i.entry "
            "WHERE p.assigned=1 AND i.name='ItemScaling reserved'");
        transaction->Append("INSERT INTO item_template SELECT * FROM mod_item_level_scaling_staged_item");
        transaction->Append("INSERT INTO scaled_item_variant SELECT * FROM mod_item_level_scaling_staged_variant");
        transaction->Append(
            "DELETE p FROM mod_item_level_scaling_slot p "
            "JOIN mod_item_level_scaling_staged_variant v ON v.variant_entry=p.entry");
        transaction->Append("DELETE FROM mod_item_level_scaling_staged_item");
        transaction->Append("DELETE FROM mod_item_level_scaling_staged_variant");
        WorldDatabase.DirectCommitTransaction(transaction);
        QueryResult remaining = WorldDatabase.Query(
            "SELECT (SELECT COUNT(*) FROM mod_item_level_scaling_staged_item)+"
            "(SELECT COUNT(*) FROM mod_item_level_scaling_staged_variant)");
        if (!remaining || remaining->Fetch()[0].Get<uint64>() != 0)
            return false;
        LOG_INFO("server.loading", "ItemScaling: promoted {} live snapshots with their original IDs.", staged);
    }
    // An assigned reservation without both payloads is corrupt, never an available slot.
    QueryResult abandoned = WorldDatabase.Query("SELECT COUNT(*) FROM mod_item_level_scaling_slot WHERE assigned<>0");
    return abandoned && abandoned->Fetch()[0].Get<uint64>() == 0;
}

bool ItemScalingLive::ReserveSlots()
{
    QueryResult existing = WorldDatabase.Query("SELECT entry FROM mod_item_level_scaling_slot WHERE assigned=0 ORDER BY entry");
    if (existing)
        do
        {
            uint32 entry = existing->Fetch()[0].Get<uint32>();
            _impl->slots.push_back(entry);
            _impl->owned.insert(entry);
            _impl->reserved.insert(entry);
        } while (existing->NextRow());
    QueryResult owned = WorldDatabase.Query(
        "SELECT COUNT(*) FROM mod_item_level_scaling_slot p JOIN item_template i ON i.entry=p.entry "
        "WHERE p.assigned=0 AND i.name='ItemScaling reserved' AND i.class=15 AND i.InventoryType=0 "
        "AND i.ItemLevel=0 AND i.RequiredLevel=0 AND i.RandomProperty=0 AND i.RandomSuffix=0 "
        "AND NOT EXISTS (SELECT 1 FROM scaled_item_variant v WHERE v.variant_entry=p.entry)");
    if (!owned || owned->Fetch()[0].Get<uint64>() != _impl->slots.size())
        return false;
    if (!sItemScalingConfig->LiveEnable || !sItemScalingConfig->Enable)
        return true;
    QueryResult high = WorldDatabase.Query(
        "SELECT GREATEST((SELECT COALESCE(MAX(entry),0) FROM item_template),"
        "(SELECT COALESCE(MAX(entry),0) FROM mod_item_level_scaling_slot)),"
        "(SELECT COALESCE(MAX(variant_entry),0) FROM scaled_item_variant)");
    if (!high)
        return false;
    uint32 highest = high->Fetch()[0].Get<uint32>();
    uint64 next = ItemScalingSafety::AllocationStart(highest, high->Fetch()[1].Get<uint32>(), sItemScalingConfig->AutoSyntheticEntry,
        sItemScalingConfig->SyntheticEntryStart, sItemScalingConfig->SyntheticEntryAutoOffset);
    while (_impl->slots.size() < sItemScalingConfig->LiveReservedSlots)
    {
        if (next > sItemScalingConfig->SyntheticEntryMaximum)
            break;
        auto transaction = WorldDatabase.BeginTransaction();
        std::vector<uint32> added;
        for (uint32 i = 0; i < 250 && _impl->slots.size() + added.size() < sItemScalingConfig->LiveReservedSlots &&
            next <= sItemScalingConfig->SyntheticEntryMaximum; ++i)
        {
            ItemTemplate placeholder{};
            placeholder.ItemId = static_cast<uint32>(next++);
            placeholder.Class = ITEM_CLASS_MISC;
            placeholder.Name1 = PlaceholderName;
            placeholder.Stackable = 1;
            transaction->Append(ItemScalingSnapshot::Insert(placeholder, "item_template"));
            transaction->Append("INSERT INTO {} (entry) VALUES ({})", SlotTable, placeholder.ItemId);
            added.push_back(placeholder.ItemId);
        }
        WorldDatabase.DirectCommitTransaction(transaction);
        QueryResult verify = WorldDatabase.Query(
            "SELECT COUNT(*) FROM mod_item_level_scaling_slot p JOIN item_template i ON i.entry=p.entry "
            "WHERE p.assigned=0 AND i.name='ItemScaling reserved'");
        if (!verify || verify->Fetch()[0].Get<uint64>() != _impl->slots.size() + added.size())
            return false;
        for (uint32 entry : added)
        {
            _impl->slots.push_back(entry);
            _impl->owned.insert(entry);
            _impl->reserved.insert(entry);
        }
    }
    LOG_INFO("server.loading", "ItemScaling: {} reserved live template slots available.", _impl->slots.size());
    return true;
}

void ItemScalingLive::IncludeOwnedEntries(std::unordered_set<uint32>& entries) const
{
    entries.insert(_impl->owned.begin(), _impl->owned.end());
}

void ItemScalingLive::Initialize()
{
    // Every reserved object must be present in both native lookup containers before gameplay.
    auto store = sObjectMgr->GetItemTemplateStoreFast();
    for (uint32 entry : _impl->slots)
    {
        if (entry >= store->size() || !(*store)[entry] || (*store)[entry]->Name1 != PlaceholderName ||
            (*store)[entry]->InventoryType != 0 || (*store)[entry]->StatsCount != 0)
        {
            LOG_ERROR("module.ItemScaling", "Live slot {} failed validation; live generation disabled.", entry);
            return;
        }
    }
    _impl->enabled = sItemScalingConfig->LiveEnable && sItemScalingRegistry->IsInitialized() &&
        !_impl->slots.empty() && _impl->LoadCatalogue();
}

uint32 ItemScalingLive::Find(VariantKey const& key) const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    auto it = _impl->requests.find(key);
    return it != _impl->requests.end() && it->second.state == Impl::State::Ready ? it->second.scaled.ItemId : 0;
}

uint32 ItemScalingLive::FindOrRequest(ItemTemplate const& base, VariantKey const& key, uint8 playerLevel)
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    if (!_impl->enabled || !sItemScalingConfig->Enable || _impl->owned.count(base.ItemId))
        return 0;
    if (sItemScalingConfig->PreserveNativeLoot &&
        ItemScalingFormula::IsNativeTargetMatch(&base, key.targetEffectiveLevel, key.targetEffectiveLevel, playerLevel))
        return 0;
    auto it = _impl->requests.find(key);
    if (it != _impl->requests.end())
        return it->second.state == Impl::State::Ready ? it->second.scaled.ItemId : 0;
    if (_impl->pendingCount >= sItemScalingConfig->LiveMaxPendingVariants || _impl->slots.empty())
    {
        ++_impl->failures;
        if (_impl->failures == 1 || _impl->failures % 100 == 0)
            LOG_WARN("module.ItemScaling", "Live capacity exhausted; original loot retained ({} slots, {} pending).",
                _impl->slots.size(), _impl->pendingCount);
        return 0;
    }
    // Consume slots at enqueue time to bound RAM and never promise more work than fits.
    Impl::Request request;
    request.base = base;
    request.scaled.ItemId = _impl->slots.front();
    _impl->slots.pop_front();
    request.playerLevel = playerLevel;
    _impl->requests.emplace(key, std::move(request));
    _impl->work.push_back(key);
    ++_impl->pendingCount;
    return 0;
}

bool ItemScalingLive::IsReserved(uint32 entry) const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    return _impl->reserved.count(entry) != 0;
}

void ItemScalingLive::BeginLoot(Loot const& loot, Player const& owner)
{
    if (loot.sourceWorldObjectGUID.IsEmpty())
        return;
    std::lock_guard<std::mutex> lock(_impl->mutex);
    // A fresh FillLoot invalidates every deferred action for the previous generation.
    _impl->sources.erase(Source(owner, loot.sourceWorldObjectGUID));
}

void ItemScalingLive::TrackLoot(Loot const& loot, Player const& owner, std::size_t index, VariantKey const& key)
{
    if (loot.sourceWorldObjectGUID.IsEmpty() || index >= loot.items.size())
        return;
    std::lock_guard<std::mutex> lock(_impl->mutex);
    auto request = _impl->requests.find(key);
    if (request == _impl->requests.end() || request->second.state == Impl::State::Failed)
        return;
    auto sourceKey = Source(owner, loot.sourceWorldObjectGUID);
    if (!_impl->sources.count(sourceKey) && _impl->sources.size() >= sItemScalingConfig->LiveMaxPendingVariants)
    {
        ++_impl->failures;
        return;
    }
    auto [it, inserted] = _impl->sources.try_emplace(sourceKey);
    if (inserted)
    {
        it->second.generation = ++_impl->generation;
        it->second.started = _impl->now;
        it->second.owner = owner.GetGUID();
    }
    auto const& item = loot.items[index];
    it->second.items.push_back({index, item.itemid, item.count, item.randomPropertyId, item.randomSuffix, key});
}

void ItemScalingLive::EnterMap(Map const& map, Player* player)
{
    if (!sItemScalingConfig->Enable || !map.IsDungeon() || map.IsBattlegroundOrArena() ||
        sItemScalingConfig->IsMapExcluded(map.GetId()))
        return;

    auto key = std::make_pair(map.GetId(), map.GetInstanceId());
    uint8 highestLevel = 0;
    std::string highestPlayerName;
    Player* owner = ItemScalingLootScript::GetEligibleOwner(&map);
    if (owner)
    {
        highestLevel = owner->GetLevel();
        highestPlayerName = owner->GetName();
    }

    bool shouldAnnounceEntry = false;
    bool shouldAnnounceRecalc = false;

    if (sItemScalingConfig->LiveGenerationMode == 1)
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        auto [it, inserted] = _impl->visits.try_emplace(key, Impl::Visit{});
        auto& visit = it->second;

        if (highestLevel > 0)
        {
            if (visit.announcedLevel == 0)
            {
                visit.announcedLevel = highestLevel;
                visit.level = highestLevel;
                visit.revision = sItemScalingConfig->Revision;
                shouldAnnounceEntry = true;
            }
            else if (highestLevel > visit.announcedLevel)
            {
                visit.announcedLevel = highestLevel;
                visit.level = highestLevel;
                visit.revision = sItemScalingConfig->Revision;
                visit.sourceIndex = 0;
                visit.itemIndex = 0;
                visit.rollIndex = 0;
                shouldAnnounceRecalc = true;
            }
            else if (player && sItemScalingConfig->Announce)
            {
                shouldAnnounceEntry = true;
            }
        }
    }

    if (sItemScalingConfig->Announce && highestLevel > 0)
    {
        char const* rawMapName = map.GetMapName();
        std::string mapName = rawMapName ? rawMapName : "Instance";
        std::string categoryTag = sItemScalingConfig->GetInstanceCategoryDescription(&map);
        uint8 ceiling = sItemScalingConfig->GetDynamicCeiling(&map);
        uint8 floor = sItemScalingConfig->GetDynamicFloor(&map);

        if (shouldAnnounceRecalc)
        {
            std::string msg = Acore::StringFormat(
                "|cff00ccff[ItemScaling]|r Player Level Update in %s %s: Loot scaling recalculated to level %u (Highest Player: %s) [Ceiling: +%u, Floor: -%u].",
                mapName.c_str(), categoryTag.c_str(), highestLevel, highestPlayerName.c_str(), ceiling, floor);

            for (auto const& ref : map.GetPlayers())
            {
                if (Player* p = ref.GetSource())
                {
                    if (p->IsInWorld() && p->GetSession())
                    {
                        ChatHandler(p->GetSession()).SendSysMessage(msg.c_str());
                    }
                }
            }
        }
        else if (shouldAnnounceEntry && player && player->IsInWorld() && player->GetSession())
        {
            std::string msg = Acore::StringFormat(
                "|cff00ccff[ItemScaling]|r Entering %s %s: Loot scaling active for level %u (Highest Player: %s) [Ceiling: +%u, Floor: -%u].",
                mapName.c_str(), categoryTag.c_str(), highestLevel, highestPlayerName.c_str(), ceiling, floor);

            ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());
        }
    }
}

void ItemScalingLive::CancelSource(uint32 map, uint32 instance, ObjectGuid source)
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    _impl->sources.erase({map, instance, source});
}

bool ItemScalingLive::DeferPacket(WorldSession* session, WorldPacket const& packet)
{
    if (!session || !session->GetPlayer())
        return false;
    Player* player = session->GetPlayer();
    if (packet.GetOpcode() == CMSG_ITEM_QUERY_SINGLE && packet.size() >= sizeof(uint32))
    {
        WorldPacket copy = packet;
        uint32 entry;
        copy >> entry;
        std::lock_guard<std::mutex> lock(_impl->mutex);
        if (!_impl->reserved.count(entry))
            return false;
        if (_impl->queries.size() < sItemScalingConfig->LiveMaxPendingVariants)
            _impl->queries.try_emplace(std::make_pair(player->GetGUID(), entry), _impl->now);
        return true;
    }
    if (packet.GetOpcode() != CMSG_LOOT || packet.size() < sizeof(uint64) || !player->IsInWorld())
        return false;
    WorldPacket copy = packet;
    ObjectGuid guid;
    copy >> guid;
    std::lock_guard<std::mutex> lock(_impl->mutex);
    auto it = _impl->sources.find(Source(*player, guid));
    if (it == _impl->sources.end())
        return false;
    if (it->second.openers.size() < 40 || it->second.openers.count(player->GetGUID()))
        it->second.openers[player->GetGUID()] = LOOT_CORPSE;
    return true;
}

bool ItemScalingLive::PrepareChest(Player* player, GameObject* go)
{
    if (!player || !go || !player->IsInWorld() || !go->IsInWorld() || !_impl->enabled ||
        !sItemScalingConfig->Enable || !sItemScalingConfig->ScaleChests || !go->GetMap()->IsDungeon() ||
        go->GetGoType() != GAMEOBJECT_TYPE_CHEST || go->GetScriptId() != 0 || !go->GetAIName().empty() ||
        !go->IsWithinDistInMap(player) || go->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE) ||
        !go->IsLootAllowedFor(player))
        return false;
    auto sourceKey = Source(*player, go->GetGUID());
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        auto it = _impl->sources.find(sourceKey);
        if (it != _impl->sources.end() && it->second.preparedChest)
        {
            if (it->second.openers.size() < 40)
                it->second.openers.try_emplace(player->GetGUID(), LOOT_CORPSE);
            return true;
        }
    }
    if (go->getLootState() != GO_READY || !go->GetGOInfo()->GetLootId() ||
        (go->GetRespawnTime() && go->isSpawnedByDefault()))
        return false;

    // Locked chests must reach the hook from the successful native OPEN_LOCK effect.
    // The initial GameObject::Use call continues normally so key/skill checks are not bypassed.
    Spell* opening = player->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    bool validatedOpen = opening && opening->GetSpellInfo()->HasEffect(SPELL_EFFECT_OPEN_LOCK) &&
        opening->m_targets.GetGOTargetGUID() == go->GetGUID() && opening->GetCastTimeRemaining() <= 0;
    if (!validatedOpen)
        return false;

    go->loot.clear();
    go->loot.sourceWorldObjectGUID = go->GetGUID();
    go->loot.sourceGameObject = go;
    Group* group = player->GetGroup();
    bool groupRules = group && go->GetGOInfo()->chest.groupLootRules;
    if (groupRules)
        group->UpdateLooterGuid(go, true);
    go->loot.FillLoot(go->GetGOInfo()->GetLootId(), LootTemplates_Gameobject, player,
        !groupRules, false, go->GetLootMode(), go);
    go->SetLootGenerationTime();
    if (groupRules && !go->loot.empty())
        group->UpdateLooterGuid(go);
    if (groupRules)
        for (ObjectGuid const& guid : go->GetAllowedLooters())
            if (Player* member = ObjectAccessor::FindPlayer(guid))
                go->loot.FillNotNormalLootFor(member);
    if (GameObjectTemplateAddon const* addon = go->GetTemplateAddon())
        go->loot.generateMoneyLoot(addon->mingold, addon->maxgold);

    LootType type = validatedOpen ? LOOT_SKINNING : LOOT_CORPSE;
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        auto it = _impl->sources.find(sourceKey);
        if (it != _impl->sources.end())
        {
            it->second.preparedChest = true;
            it->second.openers[player->GetGUID()] = type;
            return true;
        }
    }
    if (uint32 trap = go->GetGOInfo()->chest.linkedTrapId)
        go->TriggeringLinkedGameObject(trap, player);
    StartChestRolls(player, go);
    player->SendLoot(go->GetGUID(), type);
    return true;
}

bool ItemScalingLive::Impl::LoadCatalogue()
{
    // Load graph once, not from an enter-map or loot hook. Positive references are IDs.
    using Graph = std::unordered_map<uint32, std::vector<std::pair<uint32, int32>>>;
    Graph creatureLoot, chestLoot, references;
    auto load = [](char const* table, Graph& graph)
    {
        QueryResult result = WorldDatabase.Query("SELECT Entry,Item,Reference FROM {} WHERE QuestRequired=0", table);
        if (result)
            do
            {
                Field* fields = result->Fetch();
                graph[fields[0].Get<uint32>()].emplace_back(fields[1].Get<uint32>(), fields[2].Get<int32>());
            } while (result->NextRow());
    };
    load("creature_loot_template", creatureLoot);
    load("gameobject_loot_template", chestLoot);
    load("reference_loot_template", references);
    auto flatten = [&](Graph const& graph, uint32 lootId)
    {
        std::set<uint32> items;
        std::set<uint32> seen;
        std::vector<std::pair<Graph const*, uint32>> stack{{&graph, lootId}};
        while (!stack.empty())
        {
            auto [current, id] = stack.back();
            stack.pop_back();
            auto it = current->find(id);
            if (it == current->end())
                continue;
            for (auto [item, reference] : it->second)
            {
                uint32 refId = static_cast<uint32>(std::abs(reference));
                if (refId != 0 && seen.insert(refId).second)
                    stack.emplace_back(&references, refId);
                else if (reference == 0 && item)
                    items.insert(item);
            }
        }
        return std::vector<uint32>(items.begin(), items.end());
    };
    QueryResult creatures = WorldDatabase.Query(
        "SELECT DISTINCT map, id FROM creature WHERE id <> 0");
    if (creatures)
        do
        {
            Field* fields = creatures->Fetch();
            uint32 mapId = fields[0].Get<uint32>();
            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry || !mapEntry->IsDungeon())
                continue;
            if (CreatureTemplate const* base = sObjectMgr->GetCreatureTemplate(fields[1].Get<uint32>()))
            {
                std::vector<uint32> entries{base->Entry};
                for (uint32 entry : base->DifficultyEntry)
                    if (entry)
                        entries.push_back(entry);
                for (uint32 entry : entries)
                    if (CreatureTemplate const* info = sObjectMgr->GetCreatureTemplate(entry))
                        if (info->lootid && info->type != CREATURE_TYPE_CRITTER)
                            catalogue[mapId].push_back({entry, false, flatten(creatureLoot, info->lootid)});
            }
        } while (creatures->NextRow());
    QueryResult chests = WorldDatabase.Query(
        "SELECT DISTINCT g.map,t.entry,t.data1 FROM gameobject g "
        "JOIN gameobject_template t ON t.entry=g.id WHERE t.type=3");
    if (chests)
        do
        {
            Field* fields = chests->Fetch();
            MapEntry const* mapEntry = sMapStore.LookupEntry(fields[0].Get<uint32>());
            if (!mapEntry || !mapEntry->IsDungeon())
                continue;
            catalogue[fields[0].Get<uint32>()].push_back(
                {fields[1].Get<uint32>(), true, flatten(chestLoot, fields[2].Get<uint32>())});
        } while (chests->NextRow());
    QueryResult rolls = WorldDatabase.Query("SELECT entry,ench FROM item_enchantment_template WHERE chance>0");
    if (rolls)
        do
        {
            Field* fields = rolls->Fetch();
            enchantments[fields[0].Get<uint32>()].push_back(fields[1].Get<int32>());
        } while (rolls->NextRow());
    return true;
}

void ItemScalingLive::Impl::Prewarm(uint32 budget, std::map<std::pair<uint32, uint32>, Visit>& activeVisits)
{
    if (!enabled || !sItemScalingConfig->Enable || sItemScalingConfig->LiveGenerationMode != 1)
        return;
    for (auto it = activeVisits.begin(); it != activeVisits.end() && budget;)
    {
        Map* map = sMapMgr->FindMap(it->first.first, it->first.second);
        if (!map)
        {
            it = activeVisits.erase(it);
            continue;
        }
        Player* owner = ItemScalingLootScript::GetEligibleOwner(map);
        if (!owner)
        {
            ++it;
            continue;
        }
        uint8 level = owner->GetLevel();
        Visit& visit = it->second;
        if (visit.level != level || visit.revision != sItemScalingConfig->Revision)
            visit = {level, sItemScalingConfig->Revision, 0, 0, 0, visit.announcedLevel};
        auto catalogueIt = catalogue.find(map->GetId());
        if (catalogueIt != catalogue.end())
        {
            auto const& sourcesForMap = catalogueIt->second;
            while (visit.sourceIndex < sourcesForMap.size() && budget)
            {
                auto const& source = sourcesForMap[visit.sourceIndex];
                if (visit.itemIndex >= source.items.size())
                {
                    ++visit.sourceIndex;
                    visit.itemIndex = visit.rollIndex = 0;
                    continue;
                }
                uint32 entry = source.items[visit.itemIndex];
                ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
                if (!proto)
                {
                    ++visit.itemIndex;
                    visit.rollIndex = 0;
                    continue;
                }

                if (sItemScalingConfig->PreserveNativeLoot)
                {
                    CreatureTemplate const* sourceCreature = source.chest ? nullptr : sObjectMgr->GetCreatureTemplate(source.entry);
                    uint8 requestedTarget = 0;
                    uint8 bracketedTarget = 0;
                    uint8 highestPlayerLevel = 0;
                    if (ItemScalingLootScript::ResolveTargetLevels(map, owner, sourceCreature, nullptr, requestedTarget, bracketedTarget, highestPlayerLevel))
                    {
                        if (ItemScalingFormula::IsNativeTargetMatch(proto, requestedTarget, bracketedTarget, level))
                        {
                            ++visit.itemIndex;
                            visit.rollIndex = 0;
                            continue;
                        }
                    }
                }

                int32 property = 0;
                bool moreRolls = false;
                if ((proto->RandomProperty || proto->RandomSuffix) &&
                    sItemScalingConfig->RandomSuffixMode == RandomSuffixScalingMode::Bake)
                {
                    uint32 pool = static_cast<uint32>(proto->RandomProperty ? proto->RandomProperty : proto->RandomSuffix);
                    auto rolls = enchantments.find(pool);
                    if (rolls != enchantments.end() && visit.rollIndex < rolls->second.size())
                    {
                        property = rolls->second[visit.rollIndex++];
                        if (proto->RandomSuffix)
                            property = -property;
                        moreRolls = visit.rollIndex < rolls->second.size();
                    }
                }
                Loot preview;
                LootItem item{};
                item.itemid = entry;
                item.count = 1;
                item.randomPropertyId = property;
                preview.items.push_back(item);
                ItemScalingLootScript::PrepareLoot(&preview, source.chest ? LootTemplates_Gameobject : LootTemplates_Creature,
                    owner, source.chest ? nullptr : sObjectMgr->GetCreatureTemplate(source.entry), true);
                if (!moreRolls)
                {
                    ++visit.itemIndex;
                    visit.rollIndex = 0;
                }
                --budget;
            }
        }
        ++it;
    }
}

void ItemScalingLive::Update(uint32 diff)
{
    // Called after MapMgr::Update waits for map workers. Native session item queries
    // run in the world/map update phases, never concurrently with this publication phase.
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        _impl->now += diff;
    }
    _impl->callbacks.ProcessReadyCallbacks();
    for (uint32 i = 0; i < sItemScalingConfig->LiveMaxPublishPerTick; ++i)
    {
        VariantKey key;
        Impl::Request request;
        {
            std::lock_guard<std::mutex> lock(_impl->mutex);
            if (_impl->work.empty())
                break;
            key = _impl->work.front();
            _impl->work.pop_front();
            request = _impl->requests.at(key);
        }
        ItemTemplate scaled = ItemScalingFormula::CreateScaledTemplate(&request.base, request.scaled.ItemId,
            key.targetEffectiveLevel, key.targetItemLevel, key.formulaVersion, request.playerLevel, key.randomPropertyId);
        scaled.RequiredLevel = key.requiredLevel;
        if (!ItemScalingFormula::IsValidBakedTemplate(request.base, scaled, key.randomPropertyId) ||
            scaled.Name1.size() > 255 || scaled.ScriptId != 0)
        {
            std::lock_guard<std::mutex> lock(_impl->mutex);
            _impl->requests.at(key).state = Impl::State::Failed;
            --_impl->pendingCount;
            ++_impl->failures;
            LOG_WARN("module.ItemScaling", "Rejected live variant for item {}: unsupported baking or metadata.", key.baseEntry);
            continue;
        }
        auto transaction = WorldDatabase.BeginTransaction();
        transaction->Append(ItemScalingSnapshot::Insert(scaled, SnapshotTable));
        transaction->Append(StagedMapping(scaled, request.base, key));
        transaction->Append("UPDATE {} SET assigned=1 WHERE entry={} AND assigned=0", SlotTable, scaled.ItemId);
        {
            std::lock_guard<std::mutex> lock(_impl->mutex);
            _impl->requests.at(key).scaled = std::move(scaled);
        }
        auto callback = WorldDatabase.AsyncCommitTransaction(transaction);
        callback.AfterComplete([this, key](bool success)
        {
            std::lock_guard<std::mutex> lock(_impl->mutex);
            auto& entry = _impl->requests.at(key);
            entry.state = success ? Impl::State::Durable : Impl::State::Failed;
            if (success)
                _impl->durable.push_back(key);
            else
            {
                --_impl->pendingCount;
                ++_impl->failures;
                LOG_WARN("module.ItemScaling", "Live persistence failed for base {}; original loot retained.", key.baseEntry);
            }
        });
        _impl->callbacks.AddCallback(std::move(callback));
    }
    for (uint32 i = 0; i < sItemScalingConfig->LiveMaxPublishPerTick; ++i)
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        if (_impl->durable.empty())
            break;
        VariantKey key = _impl->durable.front();
        _impl->durable.pop_front();
        auto& request = _impl->requests.at(key);
        auto store = sObjectMgr->GetItemTemplateStoreFast();
        uint32 entry = request.scaled.ItemId;
        if (entry >= store->size() || !(*store)[entry] || (*store)[entry]->Name1 != PlaceholderName)
        {
            request.state = Impl::State::Failed;
            --_impl->pendingCount;
            ++_impl->failures;
            LOG_ERROR("module.ItemScaling", "Live slot {} changed unexpectedly; withholding durable snapshot.", entry);
            continue;
        }
        *(*store)[entry] = request.scaled;
        request.state = Impl::State::Ready;
        --_impl->pendingCount;
        _impl->reserved.erase(entry);
    }

    // Only the world thread edits visits. EnterMap queues under the same mutex.
    // Move them out while calling engine functions which can re-enter module hooks.
    std::map<std::pair<uint32, uint32>, Impl::Visit> visits;
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        visits.swap(_impl->visits);
    }
    _impl->Prewarm(sItemScalingConfig->LiveMaxPublishPerTick, visits);
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        for (auto const& [key, visit] : visits)
            _impl->visits.insert_or_assign(key, visit);
    }

    std::vector<std::pair<SourceKey, Impl::PendingLoot>> completed;
    std::vector<std::pair<ObjectGuid, uint32>> queryReady;
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        for (auto it = _impl->sources.begin(); it != _impl->sources.end();)
        {
            bool waiting = false;
            for (auto const& item : it->second.items)
            {
                auto const& request = _impl->requests.at(item.key);
                waiting |= request.state == Impl::State::Pending || request.state == Impl::State::Durable;
            }
            bool timedOut = _impl->now - it->second.started >= sItemScalingConfig->LiveLootWaitTimeoutMs;
            if (waiting && !timedOut)
            {
                ++it;
                continue;
            }
            if (waiting)
                ++_impl->failures;
            completed.emplace_back(it->first, std::move(it->second));
            it = _impl->sources.erase(it);
        }
        for (auto it = _impl->queries.begin(); it != _impl->queries.end();)
        {
            if (!_impl->reserved.count(it->first.second))
            {
                queryReady.push_back(it->first);
                it = _impl->queries.erase(it);
            }
            else if (_impl->now - it->second >= sItemScalingConfig->LiveLootWaitTimeoutMs)
                it = _impl->queries.erase(it);
            else
                ++it;
        }
    }
    for (auto& [source, pending] : completed)
    {
        Map* map = sMapMgr->FindMap(source.map, source.instance);
        if (!map)
            continue;
        Creature* creature = source.guid.IsCreatureOrVehicle() ? map->GetCreature(source.guid) : nullptr;
        GameObject* go = source.guid.IsGameObject() ? map->GetGameObject(source.guid) : nullptr;
        if ((!creature || !creature->IsInWorld() || creature->IsAlive()) && (!go || !go->IsInWorld()))
            continue;
        Loot* loot = creature ? &creature->loot : &go->loot;
        bool intact = loot->loot_type == LOOT_NONE;
        for (auto const& item : pending.items)
            intact = intact && item.index < loot->items.size() && !loot->items[item.index].is_looted &&
                loot->items[item.index].itemid == item.original && loot->items[item.index].count == item.count &&
                loot->items[item.index].randomPropertyId == item.property && loot->items[item.index].randomSuffix == item.suffix;
        if (!intact)
            continue; // Native/scripted loot already progressed. Never rewrite an active roll.
        bool fallback = false;
        for (auto const& item : pending.items)
        {
            uint32 entry = Find(item.key);
            if (entry && !sDisableMgr->IsDisabledFor(DISABLE_TYPE_LOOT, entry, nullptr))
            {
                loot->items[item.index].itemid = entry;
                loot->items[item.index].randomPropertyId = 0;
                loot->items[item.index].randomSuffix = 0;
            }
            else
                fallback = true;
        }
        Player* owner = ObjectAccessor::FindPlayer(pending.owner);
        if (go && pending.preparedChest)
        {
            if (!owner || !owner->IsInWorld() || owner->GetMap() != map)
            {
                owner = nullptr;
                for (auto const& [guid, type] : pending.openers)
                {
                    (void)type;
                    Player* candidate = ObjectAccessor::FindPlayer(guid);
                    if (candidate && candidate->IsInWorld() && candidate->GetMap() == map &&
                        go->IsLootAllowedFor(candidate))
                    {
                        owner = candidate;
                        break;
                    }
                }
                if (!owner)
                {
                    // Retain rolled contents and wait for an eligible opener. Never activate
                    // a group chest without initializing its rolls.
                    pending.items.clear();
                    std::lock_guard<std::mutex> lock(_impl->mutex);
                    _impl->sources.emplace(source, std::move(pending));
                    continue;
                }
            }
            if (uint32 trap = go->GetGOInfo()->chest.linkedTrapId)
                go->TriggeringLinkedGameObject(trap, owner);
            StartChestRolls(owner, go);
        }
        for (auto [guid, type] : pending.openers)
        {
            Player* player = ObjectAccessor::FindPlayer(guid);
            if (!player || !player->IsInWorld() || player->GetMap() != map)
                continue;
            if (fallback)
                ChatHandler(player->GetSession()).SendSysMessage("Item scaling could not finish; original loot preserved.");
            if (go)
            {
                if (player->IsAlive() && go->IsWithinDistInMap(player))
                    player->SendLoot(source.guid, type);
            }
            else
            {
                auto packet = new WorldPacket(CMSG_LOOT, sizeof(uint64));
                *packet << source.guid;
                player->GetSession()->QueuePacket(packet);
            }
        }
    }
    for (auto [guid, entry] : queryReady)
        if (Player* player = ObjectAccessor::FindPlayer(guid))
            if (player->IsInWorld())
            {
                auto packet = new WorldPacket(CMSG_ITEM_QUERY_SINGLE, sizeof(uint32));
                *packet << entry;
                player->GetSession()->QueuePacket(packet);
            }
}

ItemScalingLive::Diagnostics ItemScalingLive::GetDiagnostics() const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    Diagnostics result;
    result.enabled = _impl->enabled;
    result.available = _impl->slots.size();
    result.failures = _impl->failures;
    for (auto const& [key, request] : _impl->requests)
    {
        (void)key;
        result.pending += request.state == Impl::State::Pending;
        result.durable += request.state == Impl::State::Durable;
        result.ready += request.state == Impl::State::Ready;
    }
    return result;
}

namespace
{
    class ItemScalingLiveServerScript : public ServerScript
    {
    public:
        ItemScalingLiveServerScript() : ServerScript("ItemScalingLiveServerScript",
            {SERVERHOOK_CAN_PACKET_RECEIVE, SERVERHOOK_CAN_PACKET_SEND}) { }
        bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
        {
            return !sItemScalingLive->DeferPacket(session, packet);
        }
        bool CanPacketSend(WorldSession* /*session*/, WorldPacket const& packet) override
        {
            if (packet.GetOpcode() != SMSG_ITEM_QUERY_SINGLE_RESPONSE || packet.size() < sizeof(uint32))
                return true;
            WorldPacket copy = packet;
            copy.rpos(0);
            uint32 entry;
            copy >> entry;
            return !sItemScalingLive->IsReserved(entry & 0x7fffffffu);
        }
    };

    class ItemScalingLiveMapScript : public AllMapScript
    {
    public:
        ItemScalingLiveMapScript() : AllMapScript("ItemScalingLiveMapScript",
            {ALLMAPHOOK_ON_PLAYER_ENTER_ALL, ALLMAPHOOK_ON_PLAYER_LEAVE_ALL}) { }
        void OnPlayerEnterAll(Map* map, Player* player) override { sItemScalingLive->EnterMap(*map, player); }
        void OnPlayerLeaveAll(Map* map, Player* /*player*/) override { sItemScalingLive->EnterMap(*map, nullptr); }
    };

    class ItemScalingLiveCreatureScript : public AllCreatureScript
    {
    public:
        ItemScalingLiveCreatureScript() : AllCreatureScript("ItemScalingLiveCreatureScript") { }
        void OnCreatureRemoveWorld(Creature* creature) override
        {
            sItemScalingLive->CancelSource(creature->GetMapId(), creature->GetInstanceId(), creature->GetGUID());
        }
    };

    class ItemScalingLiveGameObjectScript : public AllGameObjectScript
    {
    public:
        ItemScalingLiveGameObjectScript() : AllGameObjectScript("ItemScalingLiveGameObjectScript") { }
        bool CanGameObjectGossipHello(Player* player, GameObject* go) override
        {
            return sItemScalingLive->PrepareChest(player, go);
        }
        void OnGameObjectRemoveWorld(GameObject* go) override
        {
            sItemScalingLive->CancelSource(go->GetMapId(), go->GetInstanceId(), go->GetGUID());
        }
    };

    class ItemScalingLivePlayerScript : public PlayerScript
    {
    public:
        ItemScalingLivePlayerScript() : PlayerScript("ItemScalingLivePlayerScript",
            {PLAYERHOOK_ON_LEVEL_CHANGED}) { }

        void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
        {
            if (!player || !player->IsInWorld())
                return;

            Map* map = player->GetMap();
            if (!map || !map->IsDungeon() || map->IsBattlegroundOrArena() || sItemScalingConfig->IsMapExcluded(map->GetId()))
                return;

            sItemScalingLive->EnterMap(*map, player);
        }
    };
}

void AddItemScalingLiveScripts()
{
    new ItemScalingLiveServerScript();
    new ItemScalingLiveMapScript();
    new ItemScalingLiveCreatureScript();
    new ItemScalingLiveGameObjectScript();
    new ItemScalingLivePlayerScript();
}
