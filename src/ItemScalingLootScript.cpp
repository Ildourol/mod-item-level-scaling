/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingLootScript.h"
#include "Creature.h"
#include "DBCStores.h"
#include "ItemEnchantmentMgr.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingSafety.h"
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
        if (!player)
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

void ItemScalingLootScript::OnAfterLootTemplateProcess(
    Loot* loot,
    LootTemplate const* /*tab*/,
    LootStore const& store,
    Player* lootOwner,
    bool /*personal*/,
    bool /*noEmptyError*/,
    uint16 /*lootMode*/)
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
            CreatureTemplate const* cInfo = creature->GetCreatureTemplate();
            if (cInfo)
            {
                cMin = cInfo->minlevel > 0 ? cInfo->minlevel : 1;
                cSrc = cInfo->maxlevel > 0 ? cInfo->maxlevel : cMin;
            }
        }
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

    // 3. Determine target effective scaling level (L_target) following fixed or dynamic rules
    uint8 lTarget = highestRealPlayerLevel;
    if (sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC)
    {
        // Dynamic mode: retain authentic dungeon hierarchy (Trash < Elite < Boss)
        if (isGameObjectLoot || !creature)
        {
            // For gameobjects and chests in dynamic mode, scale directly to H
            lTarget = highestRealPlayerLevel;
        }
        else
        {
            uint8 floor = sItemScalingConfig->GetDynamicFloor(map->IsRaid());
            uint8 ceiling = sItemScalingConfig->GetDynamicCeiling(map->IsRaid());

            // Check if creature was ALREADY dynamically scaled in the world by mod-autobalance.
            // Stock unmodified creature templates have level range [cMin, cSrc].
            // If creature->GetLevel() is outside [cMin, cSrc], AutoBalance has scaled it in the world.
            uint8 currentCreatureLevel = creature->GetLevel();
            bool isCreatureLevelScaled = (currentCreatureLevel < cMin || currentCreatureLevel > cSrc);

            if (isCreatureLevelScaled)
            {
                // Maintain 100% exact parity with the creature's scaled level in the world
                lTarget = currentCreatureLevel;
            }
            else
            {
                // Creature in world is unscaled (e.g. AutoBalance disabled or not loaded).
                // Calculate dynamic scaling level relative to highest real player level (H):
                // selectedLevel = (H + ceiling) - (cMax - cSrc)
                int32 delta = static_cast<int32>(cMax) - static_cast<int32>(cSrc);
                if (delta < 0)
                {
                    delta = 0;
                }

                int32 raw = (static_cast<int32>(highestRealPlayerLevel) + static_cast<int32>(ceiling)) - delta;
                int32 minLevel = static_cast<int32>(highestRealPlayerLevel) - static_cast<int32>(floor);
                int32 maxLevel = static_cast<int32>(highestRealPlayerLevel) + static_cast<int32>(ceiling);

                int32 clamped = std::clamp(raw, minLevel, maxLevel);
                lTarget = static_cast<uint8>(std::clamp<int32>(clamped, 1, 80));
            }
        }
    }
    else // SCALING_METHOD_FIXED
    {
        // Fixed mode: pinpoint the highest real player level (H) directly for all loot
        lTarget = highestRealPlayerLevel;
    }

    // Clamp to configured MinLevel and MaxLevel bounds
    lTarget = std::clamp<uint8>(lTarget, sItemScalingConfig->MinLevel, sItemScalingConfig->MaxLevel);

    if (sItemScalingConfig->Debug)
    {
        LOG_INFO("module.ItemScaling", "ItemScaling: Map {} ('{}') - Player {} (H: {}) - Mode: {} - cSrc: {} cMax: {} -> Target Level: {}",
            map->GetId(), map->GetMapName(), lootOwner->GetName(), highestRealPlayerLevel,
            sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC ? "dynamic" : "fixed",
            cSrc, cMax, lTarget);
    }

    // Use exactly the same brackets as startup generation. Never increase the target by rounding.
    uint8 requestedTarget = lTarget;
    lTarget = ItemScalingSafety::Bracket(lTarget, sItemScalingConfig->MinLevel,
        sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep);

    auto scaleLootItem = [&](LootItem& item)
    {
        ItemTemplate const* baseProto = sObjectMgr->GetItemTemplate(item.itemid);
        if (!baseProto || !ItemScalingFormula::IsScalableEquipment(baseProto))
        {
            return;
        }

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

        // Only publish templates already loaded and validated by the core.
        uint32 variantEntry = sItemScalingRegistry->FindVariant(
            baseProto,
            lTarget,
            targetIlvl,
            sItemScalingConfig->FormulaVersion,
            highestRealPlayerLevel
        );

        if (variantEntry != 0 && variantEntry != item.itemid)
        {
            item.itemid = variantEntry;

            // If item has random suffix, recalculate factor based on the new synthetic ItemLevel
            if (baseProto->RandomSuffix != 0)
            {
                item.randomSuffix = GenerateEnchSuffixFactor(variantEntry);
            }

            if (sItemScalingConfig->Debug)
            {
                LOG_INFO("module.ItemScaling", "ItemScaling: Scaled item {} '{}' -> Variant {} (Lvl: {}, Ilvl: {} vs Orig Ilvl: {})",
                    baseProto->ItemId, baseProto->Name1, variantEntry, lTarget, targetIlvl, baseProto->ItemLevel);
            }
        }
    };

    // Scale normal and quest-required drops. AzerothCore stores quest-required
    // loot in a separate vector that follows the same LootItem shape.
    for (LootItem& item : loot->items)
    {
        scaleLootItem(item);
    }

    for (LootItem& item : loot->quest_items)
    {
        scaleLootItem(item);
    }
}

void AddItemScalingLootScripts()
{
    new ItemScalingLootScript();
}
