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
    }
    else if (map->IsNonRaidDungeon())
    {
        if (!sItemScalingConfig->ScaleDungeons)
        {
            return false;
        }
        if (map->IsHeroic() && !sItemScalingConfig->ScaleHeroics)
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

    // 1. Determine highest real player level (H)
    uint8 highestRealPlayerLevel = GetHighestEligibleRealPlayerLevel(map);
    if (highestRealPlayerLevel == 0)
    {
        // No eligible players in instance: scaling disabled
        return;
    }

    // Check player level against configured MinLevel and MaxLevel bounds
    if (highestRealPlayerLevel < sItemScalingConfig->MinLevel ||
        highestRealPlayerLevel > sItemScalingConfig->MaxLevel)
    {
        return;
    }

    // 2. Resolve unmodified creature level and instance max creature level
    uint8 cMin = 0;
    uint8 cSrc = 0;
    uint8 cMax = 0;
    Creature* creature = nullptr;

    if (isCreatureLoot)
    {
        creature = map->GetCreature(loot->sourceWorldObjectGUID);
        if (creature)
        {
            if (!IsValidDropSource(creature))
            {
                return;
            }

            CreatureTemplate const* cInfo = creature->GetCreatureTemplate();
            if (cInfo)
            {
                cMin = cInfo->minlevel > 0 ? cInfo->minlevel : 1;
                cSrc = cInfo->maxlevel > 0 ? cInfo->maxlevel : cMin;
            }
        }
    }

    if (sourceOverride)
    {
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

    // 3. Resolve the target level with pure, regression-tested policy logic.
    ItemScalingTarget::Input targetInput;
    targetInput.playerLevel = highestRealPlayerLevel;
    targetInput.creatureMinLevel = cMin;
    targetInput.creatureSourceLevel = cSrc;
    targetInput.instanceMaxLevel = cMax;
    targetInput.observedCreatureLevel = creature ? creature->GetLevel() : cSrc;
    targetInput.floor = sItemScalingConfig->GetDynamicFloor(map);
    targetInput.ceiling = sItemScalingConfig->GetDynamicCeiling(map);
    targetInput.minLevel = sItemScalingConfig->MinLevel;
    targetInput.maxLevel = sItemScalingConfig->MaxLevel;
    targetInput.dynamic = sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC;
    targetInput.hasCreature = creature != nullptr || sourceOverride != nullptr;
    targetInput.realPlayersOnly = sItemScalingConfig->RealPlayersOnly;
    uint8 lTarget = ItemScalingTarget::Resolve(targetInput);

    if (sItemScalingConfig->Debug && !prewarm)
    {
        LOG_INFO("module.ItemScaling", "ItemScaling: Map {} ('{}') - Player {} (H: {}) - Mode: {} - cSrc: {} cMax: {} -> Target Level: {}",
            map->GetId(), map->GetMapName(), lootOwner->GetName(), highestRealPlayerLevel,
            sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC ? "dynamic" : "fixed",
            cSrc, cMax, lTarget);
    }

    // Bracketing is a runtime target rule. Never increase the target by rounding.
    uint8 requestedTarget = lTarget;
    lTarget = ItemScalingSafety::Bracket(lTarget, sItemScalingConfig->MinLevel,
        sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep);

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

        // Determine item's original reference level
        uint8 origRefLevel = static_cast<uint8>(baseProto->RequiredLevel);
        if (origRefLevel == 0)
        {
            origRefLevel = static_cast<uint8>(std::clamp<uint32>(baseProto->ItemLevel, 1, 80));
        }

        // Scaling does not apply when item's native level already matches target level
        // (e.g. 80 to 80, 70 to 70, 60 to 60)
        if (origRefLevel == requestedTarget || origRefLevel == lTarget)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (Native level {} matches target level {}, no scaling needed)",
                    baseProto->ItemId, baseProto->Name1, origRefLevel, lTarget);
            }
            return;
        }

        // Check if item's native level is explicitly configured as excluded
        if (sItemScalingConfig->IsLevelExcluded(origRefLevel))
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (Native level {} is in ExcludedLevels)",
                    baseProto->ItemId, baseProto->Name1, origRefLevel);
            }
            return;
        }

        // Directional scaling checks
        if (!sItemScalingConfig->ScaleDown && lTarget < origRefLevel)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (ScaleDown disabled: lTarget {} < origRef {})",
                    baseProto->ItemId, baseProto->Name1, lTarget, origRefLevel);
            }
            return;
        }
        if (!sItemScalingConfig->ScaleUp && lTarget > origRefLevel)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (ScaleUp disabled: lTarget {} > origRef {})",
                    baseProto->ItemId, baseProto->Name1, lTarget, origRefLevel);
            }
            return;
        }

        // Calculate target ItemLevel via Blizzard baseline model
        uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(baseProto, lTarget, origRefLevel);
        if (targetIlvl == 0)
        {
            return;
        }

        // If target matches original exactly, no variant needed
        if (targetIlvl == baseProto->ItemLevel && lTarget == origRefLevel)
        {
            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Skipped item {} '{}' (Already matches target lvl {} and ilvl {})",
                    baseProto->ItemId, baseProto->Name1, lTarget, targetIlvl);
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

        // A live miss is held until its durable template is published; legacy mode queues demand.
        uint32 variantEntry = sItemScalingRegistry->FindOrRequestVariant(
            baseProto,
            lTarget,
            targetIlvl,
            sItemScalingConfig->FormulaVersion,
            highestRealPlayerLevel,
            randomPropId
        );

        if (variantEntry == 0 && !prewarm)
        {
            VariantKey key{baseProto->ItemId, lTarget, targetIlvl, sItemScalingConfig->FormulaVersion,
                ITEM_SCALING_GENERATOR_REVISION,
                ItemScalingFormula::CalculateRequiredLevel(baseProto, lTarget, highestRealPlayerLevel), randomPropId};
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
                    baseProto->ItemId, baseProto->Name1, variantEntry, lTarget, targetIlvl, baseProto->ItemLevel);
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
