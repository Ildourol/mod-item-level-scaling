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
        if (sItemScalingConfig->RealPlayersOnly && session->IsBot())
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
        // No eligible real players in instance: scaling disabled
        return;
    }

    // Check player level against configured MinLevel and MaxLevel bounds
    if (highestRealPlayerLevel < sItemScalingConfig->MinLevel ||
        highestRealPlayerLevel > sItemScalingConfig->MaxLevel)
    {
        return;
    }

    // Check if player's level is explicitly excluded (e.g. 60, 70, 80)
    if (sItemScalingConfig->IsLevelExcluded(highestRealPlayerLevel))
    {
        return;
    }

    // 2. Resolve unmodified creature level and instance max creature level
    uint8 cSrc = 80;
    uint8 cMax = map->IsRaid() ? 83 : 80;

    if (isCreatureLoot)
    {
        Creature* creature = map->GetCreature(loot->sourceWorldObjectGUID);
        if (creature)
        {
            CreatureTemplate const* cInfo = creature->GetCreatureTemplate();
            if (cInfo)
            {
                cSrc = cInfo->maxlevel > 0 ? cInfo->maxlevel : cInfo->minlevel;
            }
        }

        // Determine reference max level for this instance
        LFGDungeonEntry const* dungeon = GetLFGDungeon(map->GetId(), map->GetDifficulty());
        if (dungeon && dungeon->MaxLevel > 0)
        {
            cMax = static_cast<uint8>(dungeon->MaxLevel);
        }
        if (cSrc > cMax)
        {
            cMax = cSrc;
        }
    }
    else
    {
        // For chests, represent the instance tier
        LFGDungeonEntry const* dungeon = GetLFGDungeon(map->GetId(), map->GetDifficulty());
        if (dungeon && dungeon->MaxLevel > 0)
        {
            cMax = static_cast<uint8>(dungeon->MaxLevel);
        }
        cSrc = cMax;
    }

    // If creature or instance tier is outside configured level range, leave loot default
    if (cSrc < sItemScalingConfig->MinLevel)
    {
        return;
    }
    if (sItemScalingConfig->MaxLevel < 80 && (cMax > sItemScalingConfig->MaxLevel || cSrc > sItemScalingConfig->MaxLevel))
    {
        return;
    }

    // Check if instance tier or creature matches excluded milestone levels (e.g. 60, 70, 80)
    LFGDungeonEntry const* dungeonEntry = GetLFGDungeon(map->GetId(), map->GetDifficulty());
    if (dungeonEntry && sItemScalingConfig->IsLevelExcluded(static_cast<uint8>(dungeonEntry->MaxLevel)))
    {
        return;
    }
    if (sItemScalingConfig->IsLevelExcluded(cSrc))
    {
        return;
    }
    // Handle raid/dungeon boss level offsets (+1 to +3) for excluded tiers:
    // Raid bosses: 61-63 in raids when 60 is excluded, 71-73 when 70 is excluded, 81-83 when 80 is excluded
    if (map->IsRaid())
    {
        if (cSrc <= 63 && sItemScalingConfig->IsLevelExcluded(60))
        {
            return;
        }
        if (cSrc >= 70 && cSrc <= 73 && sItemScalingConfig->IsLevelExcluded(70))
        {
            return;
        }
        if (cSrc >= 80 && sItemScalingConfig->IsLevelExcluded(80))
        {
            return;
        }
    }
    else if (cSrc >= 80 && sItemScalingConfig->IsLevelExcluded(80))
    {
        return;
    }

    // 3. Determine target effective scaling level (L_target)
    uint8 lTarget = std::clamp<uint8>(highestRealPlayerLevel, sItemScalingConfig->MinLevel, sItemScalingConfig->MaxLevel);
    if (sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC)
    {
        uint8 floor = sItemScalingConfig->GetDynamicFloor(map->IsRaid());
        uint8 ceiling = sItemScalingConfig->GetDynamicCeiling(map->IsRaid());

        int32 raw = (static_cast<int32>(highestRealPlayerLevel) + static_cast<int32>(ceiling)) -
                    (static_cast<int32>(cMax) - static_cast<int32>(cSrc));

        int32 minLevel = static_cast<int32>(highestRealPlayerLevel) - static_cast<int32>(floor);
        int32 maxLevel = static_cast<int32>(highestRealPlayerLevel) + static_cast<int32>(ceiling);

        int32 clamped = std::clamp(raw, minLevel, maxLevel);
        lTarget = static_cast<uint8>(std::clamp<int32>(clamped, sItemScalingConfig->MinLevel, sItemScalingConfig->MaxLevel));
    }

    // Ensure target level does not equal an excluded level (e.g. 60, 70, 80)
    if (sItemScalingConfig->IsLevelExcluded(lTarget))
    {
        if (lTarget > cSrc)
        {
            while (sItemScalingConfig->IsLevelExcluded(lTarget) && lTarget > cSrc && lTarget > sItemScalingConfig->MinLevel)
            {
                --lTarget;
            }
        }
        else if (lTarget < cSrc)
        {
            while (sItemScalingConfig->IsLevelExcluded(lTarget) && lTarget < cSrc && lTarget < sItemScalingConfig->MaxLevel)
            {
                ++lTarget;
            }
        }

        if (sItemScalingConfig->IsLevelExcluded(lTarget))
        {
            return;
        }
    }

    // 4. Directional scaling checks
    if (!sItemScalingConfig->ScaleDown && lTarget < cSrc)
    {
        return;
    }
    if (!sItemScalingConfig->ScaleUp && lTarget > cSrc)
    {
        return;
    }

    // 5. Scale all eligible items currently in loot->items
    for (LootItem& item : loot->items)
    {
        ItemTemplate const* baseProto = sObjectMgr->GetItemTemplate(item.itemid);
        if (!baseProto || !ItemScalingFormula::IsScalableEquipment(baseProto))
        {
            continue;
        }

        // Calculate target ItemLevel via Blizzard baseline model
        uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(baseProto, lTarget, cSrc);
        if (targetIlvl == 0)
        {
            continue;
        }

        // If target matches original exactly, no variant needed
        if (targetIlvl == baseProto->ItemLevel && lTarget == cSrc)
        {
            continue;
        }

        // Obtain or lazily create synthetic variant template
        uint32 variantEntry = sItemScalingRegistry->GetOrCreateVariant(
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
        }
    }
}

void AddItemScalingLootScripts()
{
    new ItemScalingLootScript();
}
