/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingLootScript.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "Item.h"
#include "ItemEnchantmentMgr.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingLive.h"
#include "ItemScalingSafety.h"
#include "ItemScalingTarget.h"
#include "Log.h"
#include "LootMgr.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include <algorithm>

ItemScalingLootScript::ItemScalingLootScript()
    : MiscScript("ItemScalingLootScript", { MISCHOOK_ON_AFTER_LOOT_TEMPLATE_PROCESS })
{
}

// Keep this module compatible with stock master and the Playerbot fork.
template <typename Session>
static bool IsBotSession(Session const* session)
{
    if constexpr (requires { session->IsBot(); })
        return session->IsBot();
    return false;
}

static bool IsValidDropSource(Creature const* creature)
{
    if (!creature)
        return false;
    if (creature->IsCritter() || creature->IsTotem() || creature->IsPet() || creature->IsSummon())
        return false;
    if (CreatureTemplate const* cInfo = creature->GetCreatureTemplate())
        if (cInfo->type == CREATURE_TYPE_CRITTER)
            return false;
    return true;
}

static uint8 GetHighestEligibleRealPlayerLevel(Map const* map)
{
    if (!map)
    {
        return 0;
    }

    uint8 highestLevel = 0;
    for (auto const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsInWorld())
        {
            continue;
        }

        WorldSession const* session = player->GetSession();
        if (!session)
        {
            continue;
        }

        // Strictly exclude bots from determining target level
        if (sItemScalingConfig->RealPlayersOnly && IsBotSession(session))
        {
            continue;
        }

        // Exclude GameMasters unless explicitly configured
        if (!sItemScalingConfig->IncludeGameMasters && player->IsGameMaster())
        {
            continue;
        }

        uint8 lvl = player->GetLevel();
        if (lvl > highestLevel)
        {
            highestLevel = lvl;
        }
    }

    return highestLevel;
}

static bool IsEligibleInstanceMap(Map const* map)
{
    if (!map || !map->IsDungeon())
    {
        return false;
    }

    if (map->IsBattlegroundOrArena())
    {
        return false;
    }

    if (sItemScalingConfig->IsMapExcluded(map->GetId()))
    {
        return false;
    }

    if (map->IsRaid())
    {
        if (!sItemScalingConfig->ScaleRaids)
        {
            return false;
        }
        if (map->IsHeroic() && !sItemScalingConfig->ScaleHeroics)
        {
            return false;
        }

        InstanceMap const* instanceMap = map->ToInstanceMap();
        uint32 maxPlayers = instanceMap ? instanceMap->GetMaxPlayers() : 0;
        bool isHeroic = map->IsHeroic();

        if (isHeroic)
        {
            if (maxPlayers <= 10 && !sItemScalingConfig->ScaleRaid10MHeroic)
                return false;
            if (maxPlayers > 10 && maxPlayers <= 25 && !sItemScalingConfig->ScaleRaid25MHeroic)
                return false;
        }
        else
        {
            if (maxPlayers <= 10 && !sItemScalingConfig->ScaleRaid10M)
                return false;
            if (maxPlayers == 15 && !sItemScalingConfig->ScaleRaid15M)
                return false;
            if (maxPlayers == 20 && !sItemScalingConfig->ScaleRaid20M)
                return false;
            if (maxPlayers > 20 && maxPlayers <= 25 && !sItemScalingConfig->ScaleRaid25M)
                return false;
            if (maxPlayers > 25 && maxPlayers <= 40 && !sItemScalingConfig->ScaleRaid40M)
                return false;
        }
    }
    else if (map->IsNonRaidDungeon())
    {
        if (!sItemScalingConfig->ScaleDungeons)
        {
            return false;
        }
        if (map->IsHeroic() && (!sItemScalingConfig->ScaleHeroics || !sItemScalingConfig->ScaleHeroicDungeons))
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

Player* ItemScalingLootScript::GetEligibleOwner(Map const* map)
{
    Player* owner = nullptr;
    if (!map)
        return nullptr;
    for (auto const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsInWorld() || !player->GetSession() ||
            (sItemScalingConfig->RealPlayersOnly && IsBotSession(player->GetSession())) ||
            (!sItemScalingConfig->IncludeGameMasters && player->IsGameMaster()))
            continue;
        if (!owner || owner->GetLevel() < player->GetLevel())
            owner = player;
    }
    return owner;
}

void ItemScalingLootScript::OnAfterLootTemplateProcess(
    Loot* loot,
    LootTemplate const* /*tab*/,
    LootStore const& store,
    Player* lootOwner,
    bool /*personal*/,
    bool /*noEmptyError*/,
    uint16 /*lootMode*/)
{
    PrepareLoot(loot, store, lootOwner);
}

static uint8 ResolveEffectiveCreatureLevel(
    Map const* map,
    uint8 playerLevel,
    Creature const* creature,
    CreatureTemplate const* cInfo)
{
    if (creature)
    {
        uint8 liveLevel = creature->GetLevel();
        if (cInfo)
        {
            if (liveLevel != cInfo->maxlevel && liveLevel > 0)
                return liveLevel;
        }
        else if (liveLevel > 0)
        {
            return liveLevel;
        }
    }

    uint8 bossOffset = 0;
    if (map)
    {
        bool isRaidBoss = false;
        bool isDungeonBoss = false;

        if (creature)
        {
            isRaidBoss = map->IsRaid() && creature->isWorldBoss();
            isDungeonBoss = map->IsDungeon() && (creature->IsDungeonBoss() || creature->isWorldBoss());
        }
        else if (cInfo)
        {
            isRaidBoss = map->IsRaid() && (cInfo->rank >= CREATURE_ELITE_WORLDBOSS ||
                         (cInfo->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND));
            isDungeonBoss = map->IsDungeon() && (cInfo->rank >= CREATURE_ELITE_RAREELITE ||
                            (cInfo->flags_extra & CREATURE_FLAG_EXTRA_DUNGEON_BOSS));
        }

        if (isRaidBoss)
        {
            uint32 raidCeil = sConfigMgr->GetOption<uint32>("AutoBalance.LevelScaling.DynamicLevel.Ceiling.Raids", 3);
            bossOffset = static_cast<uint8>(raidCeil > 0 ? raidCeil : 3);
        }
        else if (isDungeonBoss)
        {
            uint32 dungCeil = sConfigMgr->GetOption<uint32>("AutoBalance.LevelScaling.DynamicLevel.Ceiling.Dungeons", 2);
            bossOffset = static_cast<uint8>(dungCeil > 0 ? dungCeil : 2);
        }
    }

    if (creature)
    {
        uint8 liveLevel = creature->GetLevel();
        if (cInfo && cInfo->maxlevel >= playerLevel)
            return liveLevel;
    }

    return playerLevel + bossOffset;
}

bool ItemScalingLootScript::ResolveTargetLevels(
    Map const* map,
    Player const* lootOwner,
    CreatureTemplate const* sourceOverride,
    Creature const* creature,
    uint8& outRequestedTarget,
    uint8& outBracketedTarget,
    uint8& outHighestRealPlayerLevel,
    bool prewarm,
    ItemScalingTarget::Input* outTargetInput)
{
    if (!sItemScalingConfig->Enable || !map || !lootOwner)
    {
        return false;
    }

    if (!IsEligibleInstanceMap(map))
    {
        return false;
    }

    uint8 highestRealPlayerLevel = GetHighestEligibleRealPlayerLevel(map);
    if (highestRealPlayerLevel == 0)
    {
        return false;
    }

    if (highestRealPlayerLevel < sItemScalingConfig->MinLevel ||
        highestRealPlayerLevel > sItemScalingConfig->MaxLevel)
    {
        return false;
    }

    uint8 cMin = 0;
    uint8 cSrc = 0;
    uint8 cMax = 0;
    CreatureTemplate const* cInfo = nullptr;

    if (creature)
    {
        if (!IsValidDropSource(creature))
        {
            return false;
        }

        cInfo = creature->GetCreatureTemplate();
        if (cInfo)
        {
            cMin = cInfo->minlevel > 0 ? cInfo->minlevel : 1;
            cSrc = cInfo->maxlevel > 0 ? cInfo->maxlevel : cMin;
        }
    }

    if (sourceOverride)
    {
        cInfo = sourceOverride;
        cMin = sourceOverride->minlevel > 0 ? sourceOverride->minlevel : 1;
        cSrc = sourceOverride->maxlevel > 0 ? sourceOverride->maxlevel : cMin;
    }

    LFGDungeonEntry const* dungeon = GetLFGDungeon(map->GetId(), map->GetDifficulty());
    if (dungeon && dungeon->MaxLevel > 0)
    {
        cMax = static_cast<uint8>(dungeon->MaxLevel);
        if (cSrc == 0)
        {
            cSrc = dungeon->MinLevel > 0 ? static_cast<uint8>(dungeon->MinLevel) : cMax;
            cMin = cSrc;
        }
    }

    if (cSrc == 0)
    {
        cSrc = (creature ? creature->GetLevel() : (map->IsRaid() ? 80 : 20));
        cMin = cSrc;
    }
    if (cMax == 0 || cMax < cSrc)
    {
        cMax = cSrc;
    }

    uint8 baseFloor = sItemScalingConfig->GetDynamicFloor(map);
    uint8 baseCeiling = sItemScalingConfig->GetDynamicCeiling(map);

    int8 floorDelta = 0;
    int8 ceilingDelta = 0;

    if (!prewarm)
    {
        if (sItemScalingConfig->FloorVarianceEnable)
        {
            floorDelta = ItemScalingConfig::RollVarianceDelta(
                sItemScalingConfig->GetDynamicFloorVarianceWeights(map));
        }

        if (sItemScalingConfig->CeilingVarianceEnable)
        {
            ceilingDelta = ItemScalingConfig::RollVarianceDelta(
                sItemScalingConfig->GetDynamicCeilingVarianceWeights(map));
        }
    }

    uint8 effectiveFloor = static_cast<uint8>(std::clamp<int32>(
        static_cast<int32>(baseFloor) + floorDelta, 0, 80));
    uint8 effectiveCeiling = static_cast<uint8>(std::clamp<int32>(
        static_cast<int32>(baseCeiling) + ceilingDelta, 0, 80));

    ItemScalingTarget::Input targetInput;
    targetInput.playerLevel = highestRealPlayerLevel;
    targetInput.creatureMinLevel = cMin;
    targetInput.creatureSourceLevel = cSrc;
    targetInput.instanceMaxLevel = cMax;
    targetInput.observedCreatureLevel = ResolveEffectiveCreatureLevel(map, highestRealPlayerLevel, creature, cInfo);
    targetInput.floor = effectiveFloor;
    targetInput.ceiling = effectiveCeiling;
    targetInput.minLevel = sItemScalingConfig->MinLevel;
    targetInput.maxLevel = sItemScalingConfig->MaxLevel;
    targetInput.dynamic = sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC;
    targetInput.hasCreature = creature != nullptr || sourceOverride != nullptr;
    targetInput.realPlayersOnly = sItemScalingConfig->RealPlayersOnly;

    outRequestedTarget = ItemScalingTarget::Resolve(targetInput);
    outBracketedTarget = ItemScalingSafety::Bracket(outRequestedTarget, sItemScalingConfig->MinLevel,
        sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep);
    outHighestRealPlayerLevel = highestRealPlayerLevel;

    if (outTargetInput)
    {
        *outTargetInput = targetInput;
    }

    return true;
}

void ItemScalingLootScript::PrepareLoot(Loot* loot, LootStore const& store, Player* lootOwner,
    CreatureTemplate const* sourceOverride, bool prewarm)
{
    if (!sItemScalingConfig->Enable || !loot || !lootOwner)
    {
        return;
    }

    // Only process Creature loot (or GameObject/chest loot if enabled)
    bool isCreatureLoot = (&store == &LootTemplates_Creature);
    bool isGameObjectLoot = (&store == &LootTemplates_Gameobject);

    if (!isCreatureLoot && !isGameObjectLoot)
    {
        return;
    }

    if (isGameObjectLoot && !sItemScalingConfig->ScaleChests)
    {
        return;
    }

    Map* map = lootOwner->GetMap();
    if (!IsEligibleInstanceMap(map))
    {
        return;
    }

    if (!prewarm)
        sItemScalingLive->BeginLoot(*loot, *lootOwner);

    Creature* creature = nullptr;
    if (isCreatureLoot)
    {
        creature = map->GetCreature(loot->sourceWorldObjectGUID);
    }

    uint8 requestedTarget = 0;
    uint8 lTarget = 0;
    uint8 highestRealPlayerLevel = 0;
    ItemScalingTarget::Input baseTargetInput;
    if (!ResolveTargetLevels(map, lootOwner, sourceOverride, creature, requestedTarget, lTarget, highestRealPlayerLevel, prewarm, &baseTargetInput))
    {
        return;
    }

    if (sItemScalingConfig->Debug && !prewarm)
    {
        LOG_INFO("module.ItemScaling", "ItemScaling: Map {} ('{}') - Player {} (H: {}) - Mode: {} -> Target Level: {} (Requested: {})",
            map->GetId(), map->GetMapName(), lootOwner->GetName(), highestRealPlayerLevel,
            sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC ? "dynamic" : "fixed",
            lTarget, requestedTarget);
    }

    auto scaleLootItem = [&](LootItem& item)
    {
        if (item.needs_quest)
            return;

        ItemTemplate const* baseProto = sObjectMgr->GetItemTemplate(item.itemid);
        if (!baseProto || !ItemScalingFormula::IsScalableEquipment(baseProto))
        {
            return;
        }

        // Native scaling-distribution items cannot be represented by a fixed live snapshot.
        if (baseProto->ScalingStatDistribution != 0 || baseProto->ScalingStatValue != 0)
            return;

        // Entry-based limits/quest starters must not acquire a second identity. Conditions and
        // multi-drop bookkeeping remain attached to this LootItem; their cloned flags stay unchanged.
        if (baseProto->MaxCount != 0 || baseProto->StartQuest != 0 || baseProto->ScriptId != 0 ||
            baseProto->HasFlag(ITEM_FLAG_UNIQUE_EQUIPPABLE) ||
            sDisableMgr->IsDisabledFor(DISABLE_TYPE_LOOT, item.itemid, nullptr))
            return;

        if (!sItemScalingConfig->IsQualityEnabled(baseProto->Quality))
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (Quality {} not enabled)",
                    baseProto->ItemId, baseProto->Name1, baseProto->Quality);
            }
            return;
        }

        if (sItemScalingConfig->IsItemExcluded(baseProto->ItemId))
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (Item ID explicitly excluded)",
                    baseProto->ItemId, baseProto->Name1);
            }
            return;
        }

        uint8 effectiveRequested = requestedTarget;
        uint8 effectiveTarget = lTarget;

        if (sItemScalingConfig->VarianceScope == 1 && !prewarm &&
            (sItemScalingConfig->FloorVarianceEnable || sItemScalingConfig->CeilingVarianceEnable))
        {
            uint8 bFloor = sItemScalingConfig->GetDynamicFloor(map);
            uint8 bCeil = sItemScalingConfig->GetDynamicCeiling(map);
            int8 fDelta = sItemScalingConfig->FloorVarianceEnable
                ? ItemScalingConfig::RollVarianceDelta(sItemScalingConfig->GetDynamicFloorVarianceWeights(map))
                : 0;
            int8 cDelta = sItemScalingConfig->CeilingVarianceEnable
                ? ItemScalingConfig::RollVarianceDelta(sItemScalingConfig->GetDynamicCeilingVarianceWeights(map))
                : 0;

            ItemScalingTarget::Input itemInput = baseTargetInput;
            itemInput.floor = static_cast<uint8>(std::clamp<int32>(static_cast<int32>(bFloor) + fDelta, 0, 80));
            itemInput.ceiling = static_cast<uint8>(std::clamp<int32>(static_cast<int32>(bCeil) + cDelta, 0, 80));

            effectiveRequested = ItemScalingTarget::Resolve(itemInput);
            effectiveTarget = ItemScalingSafety::Bracket(effectiveRequested, sItemScalingConfig->MinLevel,
                sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep);
        }

        // Determine item's original reference level
        uint8 origRefLevel = ItemScalingFormula::GetNativeReferenceLevel(baseProto);

        // Scaling does not apply when item's native level already matches target level
        // (e.g. 80 to 80, 70 to 70, 60 to 60, or player level matches native in lower level instances)
        if (sItemScalingConfig->PreserveNativeLoot &&
            ItemScalingFormula::IsNativeTargetMatch(baseProto, effectiveRequested, effectiveTarget, highestRealPlayerLevel))
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Preserved original item {} '{}' (Native level {} matches target {}/bracket {}/player {}, no scaling needed)",
                    baseProto->ItemId, baseProto->Name1, origRefLevel, effectiveRequested, effectiveTarget, highestRealPlayerLevel);
            }
            return;
        }

        // Directional scaling checks
        if (!sItemScalingConfig->ScaleDown && effectiveTarget < origRefLevel)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (ScaleDown disabled: lTarget {} < origRef {})",
                    baseProto->ItemId, baseProto->Name1, effectiveTarget, origRefLevel);
            }
            return;
        }
        if (!sItemScalingConfig->ScaleUp && effectiveTarget > origRefLevel)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (ScaleUp disabled: lTarget {} > origRef {})",
                    baseProto->ItemId, baseProto->Name1, effectiveTarget, origRefLevel);
            }
            return;
        }

        // Calculate target ItemLevel via Blizzard baseline model
        uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(baseProto, effectiveTarget, origRefLevel);
        if (targetIlvl == 0)
        {
            return;
        }

        // If target matches original exactly, no variant needed
        if (targetIlvl == baseProto->ItemLevel && effectiveTarget == origRefLevel)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (Already matches target lvl {} and ilvl {})",
                    baseProto->ItemId, baseProto->Name1, effectiveTarget, targetIlvl);
            }
            return;
        }

        bool const hasRandomProps = (baseProto->RandomSuffix != 0 || baseProto->RandomProperty != 0);

        // Approach 1 (Default: Skip): Leave random suffix/property gear unscaled so original tooltip is preserved
        if (hasRandomProps && sItemScalingConfig->RandomSuffixMode == RandomSuffixScalingMode::Skip)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (RandomSuffixMode is Skip)",
                    baseProto->ItemId, baseProto->Name1);
            }
            return;
        }

        int32 randomPropId = 0;
        if (hasRandomProps && sItemScalingConfig->RandomSuffixMode == RandomSuffixScalingMode::Bake)
        {
            // Approach 2: Use the already-rolled random property ID from core, or roll it if not set yet
            randomPropId = item.randomPropertyId;
            if (randomPropId == 0)
            {
                if (prewarm)
                    return;
                randomPropId = Item::GenerateItemRandomPropertyId(baseProto->ItemId);
            }
            if (randomPropId == 0)
            {
                return;
            }
            if (!ItemScalingFormula::CanBakeRandomProperty(*baseProto, randomPropId))
                return;
        }

        // A live miss triggers asynchronous generation; loot is deferred until the durable template is published.
        uint32 variantEntry = sItemScalingRegistry->FindOrRequestVariant(
            baseProto,
            effectiveTarget,
            targetIlvl,
            sItemScalingConfig->FormulaVersion,
            highestRealPlayerLevel,
            randomPropId
        );

        if (variantEntry == 0 && !prewarm)
        {
            VariantKey key{baseProto->ItemId, effectiveTarget, targetIlvl, sItemScalingConfig->FormulaVersion,
                ITEM_SCALING_GENERATOR_REVISION,
                ItemScalingFormula::CalculateRequiredLevel(baseProto, effectiveTarget, highestRealPlayerLevel), randomPropId};
            sItemScalingLive->TrackLoot(*loot, *lootOwner, static_cast<std::size_t>(&item - loot->items.data()), key);
        }

        if (variantEntry != 0 && variantEntry != item.itemid &&
            !sDisableMgr->IsDisabledFor(DISABLE_TYPE_LOOT, variantEntry, nullptr))
        {
            item.itemid = variantEntry;

            if (randomPropId != 0)
            {
                // In Bake mode, stats & title are fixed in item_template; clear instance random fields
                item.randomSuffix = 0;
                item.randomPropertyId = 0;
            }
            else
            {
                // Match LootItem construction using the final entry for standard fixed-stat cases
                item.randomSuffix = GenerateEnchSuffixFactor(variantEntry);
                item.randomPropertyId = Item::GenerateItemRandomPropertyId(variantEntry);
            }

            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Scaled item {} '{}' -> Variant {} (Lvl: {}, Ilvl: {} vs Orig Ilvl: {})",
                    baseProto->ItemId, baseProto->Name1, variantEntry, effectiveTarget, targetIlvl, baseProto->ItemLevel);
            }
        }
    };

    // Quest-required loot must keep the entry used by HasQuestForItem().
    for (LootItem& item : loot->items)
    {
        scaleLootItem(item);
    }
}

void AddItemScalingLootScripts()
{
    new ItemScalingLootScript();
}
