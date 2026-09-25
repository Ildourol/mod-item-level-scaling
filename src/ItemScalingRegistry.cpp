/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingRegistry.h"
#include "ItemScalingSafety.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "StringFormat.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>
#include <vector>
#include <unordered_set>

ItemScalingRegistry* ItemScalingRegistry::instance()
{
    static ItemScalingRegistry instance;
    return &instance;
}

static std::string BuildItemTemplateInsertSQL(ItemTemplate const& scaledProto, uint32 baseEntry)
{
    // Clone base row in item_template and override scaled fields
    return Acore::StringFormat(
        "INSERT INTO item_template ("
        "  entry, class, subclass, SoundOverrideSubclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, InventoryType, "
        "  AllowableClass, AllowableRace, ItemLevel, RequiredLevel, RequiredSkill, RequiredSkillRank, requiredspell, requiredhonorrank, "
        "  RequiredCityRank, RequiredReputationFaction, RequiredReputationRank, maxcount, stackable, ContainerSlots, "
        "  stat_type1, stat_value1, stat_type2, stat_value2, stat_type3, stat_value3, stat_type4, stat_value4, stat_type5, stat_value5, "
        "  stat_type6, stat_value6, stat_type7, stat_value7, stat_type8, stat_value8, stat_type9, stat_value9, stat_type10, stat_value10, "
        "  ScalingStatDistribution, ScalingStatValue, dmg_min1, dmg_max1, dmg_type1, dmg_min2, dmg_max2, dmg_type2, armor, "
        "  holy_res, fire_res, nature_res, frost_res, shadow_res, arcane_res, delay, ammo_type, RangedModRange, "
        "  spellid_1, spelltrigger_1, spellcharges_1, spellppmRate_1, spellcooldown_1, spellcategory_1, spellcategorycooldown_1, "
        "  spellid_2, spelltrigger_2, spellcharges_2, spellppmRate_2, spellcooldown_2, spellcategory_2, spellcategorycooldown_2, "
        "  spellid_3, spelltrigger_3, spellcharges_3, spellppmRate_3, spellcooldown_3, spellcategory_3, spellcategorycooldown_3, "
        "  spellid_4, spelltrigger_4, spellcharges_4, spellppmRate_4, spellcooldown_4, spellcategory_4, spellcategorycooldown_4, "
        "  spellid_5, spelltrigger_5, spellcharges_5, spellppmRate_5, spellcooldown_5, spellcategory_5, spellcategorycooldown_5, "
        "  bonding, description, PageText, LanguageID, PageMaterial, startquest, lockid, Material, sheath, RandomProperty, RandomSuffix, "
        "  block, itemset, MaxDurability, area, Map, BagFamily, TotemCategory, "
        "  socketColor_1, socketContent_1, socketColor_2, socketContent_2, socketColor_3, socketContent_3, socketBonus, "
        "  GemProperties, RequiredDisenchantSkill, ArmorDamageModifier, duration, ItemLimitCategory, HolidayId, ScriptName, DisenchantID, "
        "  FoodType, minMoneyLoot, maxMoneyLoot, flagsCustom"
        ") SELECT "
        "  {} AS entry, class, subclass, SoundOverrideSubclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, InventoryType, "
        "  AllowableClass, AllowableRace, {} AS ItemLevel, {} AS RequiredLevel, RequiredSkill, RequiredSkillRank, requiredspell, requiredhonorrank, "
        "  RequiredCityRank, RequiredReputationFaction, RequiredReputationRank, maxcount, stackable, ContainerSlots, "
        "  {} AS stat_type1, {} AS stat_value1, {} AS stat_type2, {} AS stat_value2, {} AS stat_type3, {} AS stat_value3, "
        "  {} AS stat_type4, {} AS stat_value4, {} AS stat_type5, {} AS stat_value5, {} AS stat_type6, {} AS stat_value6, "
        "  {} AS stat_type7, {} AS stat_value7, {} AS stat_type8, {} AS stat_value8, {} AS stat_type9, {} AS stat_value9, "
        "  {} AS stat_type10, {} AS stat_value10, "
        "  ScalingStatDistribution, ScalingStatValue, "
        "  {} AS dmg_min1, {} AS dmg_max1, dmg_type1, {} AS dmg_min2, {} AS dmg_max2, dmg_type2, "
        "  {} AS armor, {} AS holy_res, {} AS fire_res, {} AS nature_res, {} AS frost_res, "
        "  {} AS shadow_res, {} AS arcane_res, delay, ammo_type, RangedModRange, "
        "  spellid_1, spelltrigger_1, spellcharges_1, spellppmRate_1, spellcooldown_1, spellcategory_1, spellcategorycooldown_1, "
        "  spellid_2, spelltrigger_2, spellcharges_2, spellppmRate_2, spellcooldown_2, spellcategory_2, spellcategorycooldown_2, "
        "  spellid_3, spelltrigger_3, spellcharges_3, spellppmRate_3, spellcooldown_3, spellcategory_3, spellcategorycooldown_3, "
        "  spellid_4, spelltrigger_4, spellcharges_4, spellppmRate_4, spellcooldown_4, spellcategory_4, spellcategorycooldown_4, "
        "  spellid_5, spelltrigger_5, spellcharges_5, spellppmRate_5, spellcooldown_5, spellcategory_5, spellcategorycooldown_5, "
        "  bonding, description, PageText, LanguageID, PageMaterial, startquest, lockid, Material, sheath, RandomProperty, RandomSuffix, "
        "  {} AS block, itemset, MaxDurability, area, Map, BagFamily, TotemCategory, "
        "  socketColor_1, socketContent_1, socketColor_2, socketContent_2, socketColor_3, socketContent_3, socketBonus, "
        "  GemProperties, RequiredDisenchantSkill, ArmorDamageModifier, duration, ItemLimitCategory, HolidayId, ScriptName, DisenchantID, "
        "  FoodType, minMoneyLoot, maxMoneyLoot, flagsCustom "
        "FROM item_template WHERE entry = {};",
        scaledProto.ItemId,
        scaledProto.ItemLevel,
        scaledProto.RequiredLevel,
        scaledProto.ItemStat[0].ItemStatType, scaledProto.ItemStat[0].ItemStatValue,
        scaledProto.ItemStat[1].ItemStatType, scaledProto.ItemStat[1].ItemStatValue,
        scaledProto.ItemStat[2].ItemStatType, scaledProto.ItemStat[2].ItemStatValue,
        scaledProto.ItemStat[3].ItemStatType, scaledProto.ItemStat[3].ItemStatValue,
        scaledProto.ItemStat[4].ItemStatType, scaledProto.ItemStat[4].ItemStatValue,
        scaledProto.ItemStat[5].ItemStatType, scaledProto.ItemStat[5].ItemStatValue,
        scaledProto.ItemStat[6].ItemStatType, scaledProto.ItemStat[6].ItemStatValue,
        scaledProto.ItemStat[7].ItemStatType, scaledProto.ItemStat[7].ItemStatValue,
        scaledProto.ItemStat[8].ItemStatType, scaledProto.ItemStat[8].ItemStatValue,
        scaledProto.ItemStat[9].ItemStatType, scaledProto.ItemStat[9].ItemStatValue,
        scaledProto.Damage[0].DamageMin, scaledProto.Damage[0].DamageMax,
        scaledProto.Damage[1].DamageMin, scaledProto.Damage[1].DamageMax,
        scaledProto.Armor,
        scaledProto.HolyRes, scaledProto.FireRes, scaledProto.NatureRes,
        scaledProto.FrostRes, scaledProto.ShadowRes, scaledProto.ArcaneRes,
        scaledProto.Block,
        baseEntry
    );
}

namespace
{
    // All fields consumed by CreateScaledTemplate; other item attributes are cloned by INSERT ... SELECT.
    std::string const BaseColumns =
        "b.entry,b.class,b.subclass,b.Quality,b.InventoryType,b.ItemLevel,b.RequiredLevel,"
        "b.stat_type1,b.stat_value1,b.stat_type2,b.stat_value2,b.stat_type3,b.stat_value3,"
        "b.stat_type4,b.stat_value4,b.stat_type5,b.stat_value5,b.stat_type6,b.stat_value6,"
        "b.stat_type7,b.stat_value7,b.stat_type8,b.stat_value8,b.stat_type9,b.stat_value9,"
        "b.stat_type10,b.stat_value10,b.dmg_min1,b.dmg_max1,b.dmg_type1,b.dmg_min2,b.dmg_max2,"
        "b.dmg_type2,b.armor,b.delay,b.block,b.holy_res,b.fire_res,b.nature_res,b.frost_res,"
        "b.shadow_res,b.arcane_res,b.ScalingStatDistribution,b.ScalingStatValue";

    ItemTemplate ReadBaseTemplate(Field* fields)
    {
        ItemTemplate item{};
        item.ItemId = fields[0].Get<uint32>();
        item.Class = fields[1].Get<uint8>();
        item.SubClass = fields[2].Get<uint8>();
        item.Quality = fields[3].Get<uint8>();
        item.InventoryType = fields[4].Get<uint8>();
        item.ItemLevel = fields[5].Get<uint16>();
        item.RequiredLevel = fields[6].Get<uint8>();
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            int32 value = fields[8 + i * 2].Get<int32>();
            if (!value)
                continue;
            auto& stat = item.ItemStat[item.StatsCount++];
            stat.ItemStatType = fields[7 + i * 2].Get<uint8>();
            stat.ItemStatValue = value;
        }
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            item.Damage[i].DamageMin = fields[27 + i * 3].Get<float>();
            item.Damage[i].DamageMax = fields[28 + i * 3].Get<float>();
            item.Damage[i].DamageType = fields[29 + i * 3].Get<uint8>();
        }
        item.Armor = fields[33].Get<uint32>();
        item.Delay = fields[34].Get<uint16>();
        item.Block = fields[35].Get<uint32>();
        item.HolyRes = fields[36].Get<int16>();
        item.FireRes = fields[37].Get<int16>();
        item.NatureRes = fields[38].Get<int16>();
        item.FrostRes = fields[39].Get<int16>();
        item.ShadowRes = fields[40].Get<int16>();
        item.ArcaneRes = fields[41].Get<int16>();
        item.ScalingStatDistribution = fields[42].Get<uint16>();
        item.ScalingStatValue = fields[43].Get<uint32>();
        return item;
    }

    VariantKey ReadKey(Field* fields)
    {
        return {fields[0].Get<uint32>(), fields[1].Get<uint8>(), fields[2].Get<uint16>(),
            fields[3].Get<uint8>(), fields[4].Get<uint8>()};
    }

    bool ValidKey(VariantKey const& key)
    {
        return key.baseEntry != 0 && key.targetEffectiveLevel >= 1 && key.targetEffectiveLevel <= 80 &&
            key.targetItemLevel >= 1 && key.targetItemLevel <= 300 && key.formulaVersion != 0 &&
            key.requiredLevel >= 1 && key.requiredLevel <= 80;
    }

    std::string VariantInsert(uint32 entry, VariantKey const& key)
    {
        return Acore::StringFormat(
            "INSERT INTO scaled_item_variant "
            "(variant_entry,base_entry,target_effective_level,target_item_level,formula_version,required_level) "
            "VALUES ({},{},{},{},{},{});", entry, key.baseEntry, key.targetEffectiveLevel,
            key.targetItemLevel, key.formulaVersion, key.requiredLevel);
    }

    // DirectCommitTransaction has no success return. Verify each committed batch before proceeding.
    bool CommitBatch(WorldDatabaseTransaction& transaction, std::vector<uint32>& entries)
    {
        if (entries.empty())
            return true;
        WorldDatabase.DirectCommitTransaction(transaction);
        std::string ids;
        for (uint32 entry : entries)
        {
            if (!ids.empty())
                ids += ',';
            ids += std::to_string(entry);
        }
        QueryResult result = WorldDatabase.Query(
            "SELECT COUNT(*) FROM scaled_item_variant s "
            "JOIN item_template i ON i.entry=s.variant_entry "
            "WHERE s.variant_entry IN ({}) AND i.ItemLevel=s.target_item_level "
            "AND i.RequiredLevel=s.required_level", ids);
        if (!result || result->Fetch()[0].Get<uint64>() != entries.size())
        {
            LOG_ERROR("module.ItemScaling", "ItemScaling startup transaction verification failed; scaling disabled.");
            return false;
        }
        entries.clear();
        transaction = WorldDatabase.BeginTransaction();
        return true;
    }
}

bool ItemScalingRegistry::ValidateSchema()
{
    QueryResult columns = WorldDatabase.Query(
        "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant'");
    if (!columns)
    {
        LOG_ERROR("module.ItemScaling",
            "Missing scaled_item_variant table. Apply module database updates before starting worldserver.");
        return false;
    }

    std::unordered_set<std::string> names;
    do
    {
        Field* field = columns->Fetch();
        std::string name = field[0].Get<std::string>();
        std::string type = field[1].Get<std::string>();
        std::string columnType = field[2].Get<std::string>();
        std::string expectedType;

        if (name == "variant_entry" || name == "base_entry")
            expectedType = "int";
        else if (name == "target_item_level")
            expectedType = "smallint";
        else if (name == "target_effective_level" || name == "formula_version" || name == "required_level")
            expectedType = "tinyint";

        if (!expectedType.empty() && (type != expectedType || columnType.find("unsigned") == std::string::npos))
        {
            LOG_ERROR("module.ItemScaling",
                "Incompatible module column {}: {}. Apply the current module database updates.",
                name, columnType);
            return false;
        }

        names.insert(name);
    } while (columns->NextRow());

    for (char const* required : {"variant_entry", "base_entry", "target_effective_level",
        "target_item_level", "formula_version", "required_level"})
    {
        if (!names.count(required))
        {
            LOG_ERROR("module.ItemScaling",
                "Incompatible scaled_item_variant schema: missing {}. Apply the current module database updates.",
                required);
            return false;
        }
    }

    QueryResult engines = WorldDatabase.Query(
        "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME IN ('item_template','scaled_item_variant') AND ENGINE='InnoDB'");
    if (!engines || engines->Fetch()[0].Get<uint64>() != 2)
    {
        LOG_ERROR("module.ItemScaling", "ItemScaling requires InnoDB for template and registry transactions.");
        return false;
    }

    QueryResult primary = WorldDatabase.Query(
        "SELECT COLUMN_NAME FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='PRIMARY' ORDER BY SEQ_IN_INDEX");
    if (!primary || primary->GetRowCount() != 1 || primary->Fetch()[0].Get<std::string>() != "variant_entry")
    {
        LOG_ERROR("module.ItemScaling",
            "Incompatible module table primary key. Apply the current module database updates.");
        return false;
    }

    QueryResult index = WorldDatabase.Query(
        "SELECT COLUMN_NAME,NON_UNIQUE FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key' ORDER BY SEQ_IN_INDEX");
    std::vector<std::string> indexColumns;
    bool indexIsUnique = true;
    if (index)
    {
        do
        {
            Field* field = index->Fetch();
            indexColumns.push_back(field[0].Get<std::string>());
            indexIsUnique = indexIsUnique && field[1].Get<uint8>() == 0;
        } while (index->NextRow());
    }

    std::vector<std::string> expected = {"base_entry", "target_effective_level", "target_item_level",
        "formula_version", "required_level"};
    if (!indexIsUnique || indexColumns != expected)
    {
        LOG_ERROR("module.ItemScaling",
            "Incompatible scaled_item_variant unique key. Apply the current module database updates.");
        return false;
    }

    return true;
}

bool ItemScalingRegistry::ResolveSyntheticEntryRange()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT COALESCE((SELECT MAX(entry) FROM item_template),0),"
        "COALESCE((SELECT MAX(variant_entry) FROM scaled_item_variant),0)");
    if (!result)
        return false;
    uint64 highestItem = result->Fetch()[0].Get<uint64>();
    uint64 highestVariant = result->Fetch()[1].Get<uint64>();
    _nextSyntheticEntry = ItemScalingSafety::AllocationStart(highestItem, highestVariant,
        sItemScalingConfig->AutoSyntheticEntry, sItemScalingConfig->SyntheticEntryStart,
        sItemScalingConfig->SyntheticEntryAutoOffset);
    // Do not overwrite configuration with allocator state. Existing IDs are never renumbered.
    if (_nextSyntheticEntry > sItemScalingConfig->SyntheticEntryMaximum)
    {
        LOG_WARN("module.ItemScaling", "Synthetic ID limit reached; no new variants will be staged.");
    }
    return true;
}

bool ItemScalingRegistry::SynchronizeExistingVariants()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT s.variant_entry,s.base_entry,s.target_effective_level,s.target_item_level,"
        "s.formula_version,s.required_level,{} FROM scaled_item_variant s "
        "JOIN item_template b ON b.entry=s.base_entry "
        "LEFT JOIN item_template i ON i.entry=s.variant_entry WHERE i.entry IS NULL", BaseColumns);
    if (!result)
        return true;
    auto transaction = WorldDatabase.BeginTransaction();
    std::vector<uint32> entries;
    do
    {
        Field* fields = result->Fetch();
        uint32 entry = fields[0].Get<uint32>();
        VariantKey key = ReadKey(fields + 1);
        if (!ValidKey(key) || !entry || entry == key.baseEntry ||
            entry > sItemScalingConfig->SyntheticEntryMaximum)
        {
            LOG_ERROR("module.ItemScaling", "Invalid persisted variant {} skipped during recovery.", entry);
            continue;
        }
        ItemTemplate base = ReadBaseTemplate(fields + 6);
        ItemTemplate scaled = ItemScalingFormula::CreateScaledTemplate(&base, entry,
            key.targetEffectiveLevel, key.targetItemLevel, key.formulaVersion, key.requiredLevel);
        scaled.RequiredLevel = key.requiredLevel;
        transaction->Append(BuildItemTemplateInsertSQL(scaled, key.baseEntry));
        entries.push_back(entry);
        if (entries.size() >= 250 && !CommitBatch(transaction, entries))
            return false;
    } while (result->NextRow());
    return CommitBatch(transaction, entries);
}

bool ItemScalingRegistry::PreStageDungeonLoot()
{
    if (!sItemScalingConfig->PreStageDungeonLoot)
    {
        LOG_WARN("module.ItemScaling", "Pre-staging disabled: only existing persisted variants can be used.");
        return true;
    }
    // Both stock master and the Playerbot branch use id1/id2/id3 for creature spawn alternatives.
    QueryResult roots = WorldDatabase.Query(
        "SELECT clt.Item,clt.Reference FROM creature cr "
        "JOIN instance_template inst ON inst.map=cr.map "
        "JOIN (SELECT guid AS spawnId,id AS entry FROM creature "
        "UNION SELECT spawnId,entry FROM creature_multispawn) spawn_ct ON spawn_ct.spawnId=cr.guid "
        "JOIN creature_template base_ct ON base_ct.entry=spawn_ct.entry "
        "JOIN creature_template ct ON ct.entry IN (base_ct.entry,base_ct.difficulty_entry_1,"
        "base_ct.difficulty_entry_2,base_ct.difficulty_entry_3) "
        "JOIN creature_loot_template clt ON clt.Entry=ct.lootid "
        "UNION SELECT glt.Item,glt.Reference FROM gameobject go "
        "JOIN instance_template inst ON inst.map=go.map "
        "JOIN gameobject_template gt ON gt.entry=go.id "
        "JOIN gameobject_loot_template glt ON glt.Entry=gt.Data1 WHERE gt.type IN (3,25)");
    if (!roots)
        return true;

    std::unordered_set<uint32> itemIds;
    std::vector<uint32> references;
    auto collect = [&](uint32 item, int32 reference)
    {
        if (reference)
            references.push_back(static_cast<uint32>(std::abs(reference)));
        else if (item)
            itemIds.insert(item);
    };
    do
    {
        Field* fields = roots->Fetch();
        collect(fields[0].Get<uint32>(), fields[1].Get<int32>());
    } while (roots->NextRow());
    std::unordered_map<uint32, std::vector<std::pair<uint32, uint32>>> referenceRows;
    QueryResult rows = WorldDatabase.Query("SELECT Entry,Item,Reference FROM reference_loot_template");
    if (rows)
    {
        do
        {
            Field* fields = rows->Fetch();
            referenceRows[fields[0].Get<uint32>()].emplace_back(
                fields[1].Get<uint32>(), fields[2].Get<int32>());
        } while (rows->NextRow());
    }
    std::unordered_set<uint32> visited;
    while (!references.empty())
    {
        uint32 reference = references.back();
        references.pop_back();
        if (!visited.insert(reference).second)
            continue;
        auto it = referenceRows.find(reference);
        if (it != referenceRows.end())
            for (auto const& [item, nested] : it->second)
                collect(item, nested);
    }

    std::set<VariantKey> existing;
    QueryResult variants = WorldDatabase.Query(
        "SELECT base_entry,target_effective_level,target_item_level,formula_version,required_level "
        "FROM scaled_item_variant");
    if (variants)
    {
        do
        {
            existing.insert(ReadKey(variants->Fetch()));
        } while (variants->NextRow());
    }
    QueryResult bases = WorldDatabase.Query(
        "SELECT {} FROM item_template b LEFT JOIN scaled_item_variant s ON s.variant_entry=b.entry "
        "WHERE s.variant_entry IS NULL AND b.class IN (2,4) ORDER BY b.entry", BaseColumns);
    if (!bases)
        return true;
    auto transaction = WorldDatabase.BeginTransaction();
    std::vector<uint32> entries;
    uint32 created = 0;
    do
    {
        ItemTemplate base = ReadBaseTemplate(bases->Fetch());
        if (!itemIds.count(base.ItemId) || !ItemScalingFormula::IsScalableEquipment(&base))
            continue;
        uint8 originalLevel = base.RequiredLevel ? static_cast<uint8>(base.RequiredLevel) :
            static_cast<uint8>(std::clamp<uint32>(base.ItemLevel, 1, 80));
        if (sItemScalingConfig->IsLevelExcluded(originalLevel))
            continue;
        for (uint32 target = sItemScalingConfig->MinLevel; target <= sItemScalingConfig->MaxLevel;
            ++target)
        {
            if (ItemScalingSafety::Bracket(target, sItemScalingConfig->MinLevel,
                sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep) != target)
                continue;
            if (target == originalLevel || (!sItemScalingConfig->ScaleUp && target > originalLevel) ||
                (!sItemScalingConfig->ScaleDown && target < originalLevel))
                continue;
            uint16 ilvl = sItemScalingBaseline->CalculateTargetItemLevel(&base, target, originalLevel);
            // Cover the configured dynamic window and bracket rounding. Unusual external scaling safely misses.
            uint32 floor = std::max(sItemScalingConfig->DynamicFloorDungeons, sItemScalingConfig->DynamicFloorRaids);
            uint32 ceiling = std::max(sItemScalingConfig->DynamicCeilingDungeons, sItemScalingConfig->DynamicCeilingRaids);
            uint32 firstPlayer = target > ceiling ? target - ceiling : 1;
            uint32 lastPlayer = std::min<uint32>(80, target + floor + sItemScalingConfig->BracketStep - 1);
            if (sItemScalingConfig->Method == SCALING_METHOD_FIXED)
            {
                firstPlayer = target;
                lastPlayer = std::min<uint32>(80, target + sItemScalingConfig->BracketStep - 1);
            }
            firstPlayer = std::max<uint32>(firstPlayer, sItemScalingConfig->MinLevel);
            lastPlayer = std::min<uint32>(lastPlayer, sItemScalingConfig->MaxLevel);
            for (uint32 playerLevel = firstPlayer; playerLevel <= lastPlayer; ++playerLevel)
            {
                uint8 required = ItemScalingFormula::CalculateRequiredLevel(&base, target, playerLevel);
                VariantKey key{base.ItemId, static_cast<uint8>(target), ilvl,
                    sItemScalingConfig->FormulaVersion, required};
                if (!ValidKey(key) || existing.count(key))
                    continue;
                if (created >= sItemScalingConfig->MaxNewVariantsPerStartup ||
                    _nextSyntheticEntry > sItemScalingConfig->SyntheticEntryMaximum)
                {
                    LOG_WARN("module.ItemScaling", "Startup generation limit reached after {} new variants. "
                        "Unstaged drops retain their base items; later restarts can stage remaining variants.", created);
                    return CommitBatch(transaction, entries);
                }
                uint32 entry = static_cast<uint32>(_nextSyntheticEntry++);
                ItemTemplate scaled = ItemScalingFormula::CreateScaledTemplate(&base, entry, target,
                    ilvl, key.formulaVersion, playerLevel);
                transaction->Append(BuildItemTemplateInsertSQL(scaled, base.ItemId));
                transaction->Append(VariantInsert(entry, key));
                entries.push_back(entry);
                existing.insert(key);
                ++created;
                if (entries.size() >= 250 && !CommitBatch(transaction, entries))
                    return false;
            }
        }
    } while (bases->NextRow());
    LOG_INFO("server.loading", "ItemScaling: staged {} new variants.", created);
    return CommitBatch(transaction, entries);
}

void ItemScalingRegistry::OnLoadCustomDatabaseTable()
{
    if (_dbSynchronized)
        return;
    if (!ValidateSchema() || !ResolveSyntheticEntryRange() ||
        !ItemScalingFormula::LoadStartupCurves(sWorld->GetDataPath()))
    {
        LOG_ERROR("module.ItemScaling", "Startup prerequisites failed; item scaling disabled for this run.");
        return;
    }
    sItemScalingBaseline->BuildBaseline();
    _dbSynchronized = SynchronizeExistingVariants() && PreStageDungeonLoot();
}

void ItemScalingRegistry::Initialize()
{
    if (!_dbSynchronized || _initialized.load())
        return;
    QueryResult result = WorldDatabase.Query(
        "SELECT variant_entry,base_entry,target_effective_level,target_item_level,formula_version,required_level "
        "FROM scaled_item_variant");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 entry = fields[0].Get<uint32>();
            VariantKey key = ReadKey(fields + 1);
            ItemTemplate const* item = sObjectMgr->GetItemTemplate(entry);
            if (!ValidKey(key) || entry == key.baseEntry || !item ||
                !sObjectMgr->GetItemTemplate(key.baseEntry) || item->RequiredLevel != key.requiredLevel ||
                item->ItemLevel != key.targetItemLevel)
            {
                LOG_ERROR("module.ItemScaling", "Persisted variant {} failed validation; not used for new loot.", entry);
                continue;
            }
            _keyToEntry.emplace(key, entry);
        } while (result->NextRow());
    }
    _initialized.store(true);
    LOG_INFO("server.loading", "ItemScaling: indexed {} validated variants; gameplay is lookup-only.", _keyToEntry.size());
}

uint32 ItemScalingRegistry::FindVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel,
    uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel) const
{
    if (!baseProto || !_initialized.load())
        return 0;
    uint8 required = ItemScalingFormula::CalculateRequiredLevel(baseProto, targetEffectiveLevel, highestRealPlayerLevel);
    VariantKey key{baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion, required};
    auto it = _keyToEntry.find(key);
    return it == _keyToEntry.end() ? 0 : it->second;
}
