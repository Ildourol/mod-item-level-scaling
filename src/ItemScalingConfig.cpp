/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingConfig.h"
#include "ItemScalingSafety.h"
#include "Tokenize.h"
#include "StringConvert.h"
#include "Log.h"
#include "Map.h"
#include <string_view>
#include <algorithm>

ItemScalingConfig* ItemScalingConfig::instance()
{
    static ItemScalingConfig instance;
    return &instance;
}

void ItemScalingConfig::Load(bool reload)
{
    ++Revision;
    if (reload)
    {
        if (sConfigMgr->GetOption<bool>("ItemScaling.Live.Enable", true) != LiveEnable ||
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.GenerationMode", 1) != LiveGenerationMode ||
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.ReservedSlots", 4096) != LiveReservedSlots ||
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.MaxPendingVariants", 4096) != LiveMaxPendingVariants ||
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.MaxPublishPerTick", 64) != LiveMaxPublishPerTick ||
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.LootWaitTimeoutMs", 10000) != LiveLootWaitTimeoutMs)
            LOG_WARN("module.ItemScaling", "ItemScaling: Live.* settings require restart; active values retained.");
        uint8 newFormula = static_cast<uint8>(std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.FormulaVersion", 1), 1, 255));
        bool newDemand = sConfigMgr->GetOption<bool>("ItemScaling.DemandLedger.Enable", true);
        bool newPreserve = sConfigMgr->GetOption<bool>("ItemScaling.PreserveNonZeroStats", true);
        uint32 newRandomMode = sConfigMgr->GetOption<uint32>("ItemScaling.RandomSuffix.Mode", 0);
        RandomSuffixScalingMode expectedRandomMode = (newRandomMode == 1) ? RandomSuffixScalingMode::Bake : RandomSuffixScalingMode::Skip;

        if (newFormula != FormulaVersion || newDemand != DemandLedgerEnable || newPreserve != PreserveNonZeroStats || expectedRandomMode != RandomSuffixMode)
        {
            LOG_WARN("module.ItemScaling",
                "ItemScaling: Schema invariant options (FormulaVersion, PreserveNonZeroStats, DemandLedger.Enable, RandomSuffix.Mode) "
                "cannot be changed live and require a worldserver restart. Current persistent invariants remain active.");
        }
    }
    else
    {
        PreserveNonZeroStats = sConfigMgr->GetOption<bool>("ItemScaling.PreserveNonZeroStats", true);
        uint32 randomModeVal = sConfigMgr->GetOption<uint32>("ItemScaling.RandomSuffix.Mode", 0);
        RandomSuffixMode = (randomModeVal == 1) ? RandomSuffixScalingMode::Bake : RandomSuffixScalingMode::Skip;
    }

    Enable = sConfigMgr->GetOption<bool>("ItemScaling.Enable", true);
    ScaleDungeons = sConfigMgr->GetOption<bool>("ItemScaling.ScaleDungeons", true);
    ScaleRaids = sConfigMgr->GetOption<bool>("ItemScaling.ScaleRaids", true);
    ScaleHeroics = sConfigMgr->GetOption<bool>("ItemScaling.ScaleHeroics", true);
    ScaleHeroicDungeons = sConfigMgr->GetOption<bool>("ItemScaling.Scale.HeroicDungeons", ScaleHeroics);
    ScaleRaid10M = sConfigMgr->GetOption<bool>("ItemScaling.Scale.Raid10M", true);
    ScaleRaid10MHeroic = sConfigMgr->GetOption<bool>("ItemScaling.Scale.Raid10MHeroic", true);
    ScaleRaid15M = sConfigMgr->GetOption<bool>("ItemScaling.Scale.Raid15M", true);
    ScaleRaid20M = sConfigMgr->GetOption<bool>("ItemScaling.Scale.Raid20M", true);
    ScaleRaid25M = sConfigMgr->GetOption<bool>("ItemScaling.Scale.Raid25M", true);
    ScaleRaid25MHeroic = sConfigMgr->GetOption<bool>("ItemScaling.Scale.Raid25MHeroic", true);
    ScaleRaid40M = sConfigMgr->GetOption<bool>("ItemScaling.Scale.Raid40M", true);
    ScaleChests = sConfigMgr->GetOption<bool>("ItemScaling.ScaleChests", true);

    std::string methodStr = sConfigMgr->GetOption<std::string>("ItemScaling.LevelScaling.Method", "dynamic");
    if (methodStr == "fixed")
    {
        Method = SCALING_METHOD_FIXED;
    }
    else
    {
        Method = SCALING_METHOD_DYNAMIC;
    }

    auto const levels = ItemScalingSafety::LevelRange(
        sConfigMgr->GetOption<uint32>("ItemScaling.MinLevel", 1),
        sConfigMgr->GetOption<uint32>("ItemScaling.MaxLevel", 80));
    MinLevel = levels.first;
    MaxLevel = levels.second;

    RealPlayersOnly = sConfigMgr->GetOption<bool>("ItemScaling.RealPlayersOnly", true);
    IncludeGameMasters = sConfigMgr->GetOption<bool>("ItemScaling.IncludeGameMasters", false);
    ScaleUp = sConfigMgr->GetOption<bool>("ItemScaling.ScaleUp", true);
    ScaleDown = sConfigMgr->GetOption<bool>("ItemScaling.ScaleDown", true);
    ScaleExistingScalingItems = sConfigMgr->GetOption<bool>("ItemScaling.ScaleExistingScalingItems", false);

    ScalePoor = sConfigMgr->GetOption<bool>("ItemScaling.ScalePoor", true);
    ScaleCommon = sConfigMgr->GetOption<bool>("ItemScaling.ScaleCommon", true);
    ScaleUncommon = sConfigMgr->GetOption<bool>("ItemScaling.ScaleUncommon", true);
    ScaleRare = sConfigMgr->GetOption<bool>("ItemScaling.ScaleRare", true);
    ScaleEpic = sConfigMgr->GetOption<bool>("ItemScaling.ScaleEpic", true);
    ScaleLegendary = sConfigMgr->GetOption<bool>("ItemScaling.ScaleLegendary", true);
    ScaleArtifact = sConfigMgr->GetOption<bool>("ItemScaling.ScaleArtifact", true);
    ScaleHeirloom = sConfigMgr->GetOption<bool>("ItemScaling.ScaleHeirloom", false);

    std::string reqPolicyStr = sConfigMgr->GetOption<std::string>("ItemScaling.RequiredLevel.Policy", "target");
    if (reqPolicyStr == "player")
    {
        ReqLevelPolicy = REQ_POLICY_PLAYER;
    }
    else if (reqPolicyStr == "target")
    {
        ReqLevelPolicy = REQ_POLICY_TARGET;
    }
    else
    {
        ReqLevelPolicy = REQ_POLICY_TARGET_CAPPED_PLAYER;
    }

    DynamicCeilingDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Dungeons", 5)));
    DynamicFloorDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Dungeons", 3)));
    DynamicCeilingRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raids", 3)));
    DynamicFloorRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raids", 0)));

    DynamicCeilingHeroicDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.HeroicDungeons", 5)));
    DynamicFloorHeroicDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.HeroicDungeons", 0)));

    DynamicCeilingHeroicDungeonsTBC = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.HeroicDungeons.TBC", DynamicCeilingHeroicDungeons)));
    DynamicFloorHeroicDungeonsTBC = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.HeroicDungeons.TBC", DynamicFloorHeroicDungeons)));

    DynamicCeilingHeroicDungeonsWrath = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.HeroicDungeons.Wrath", DynamicCeilingHeroicDungeons)));
    DynamicFloorHeroicDungeonsWrath = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.HeroicDungeons.Wrath", DynamicFloorHeroicDungeons)));

    DynamicCeilingHeroicRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.HeroicRaids", DynamicCeilingRaids)));
    DynamicFloorHeroicRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.HeroicRaids", DynamicFloorRaids)));

    DynamicCeilingRaid10M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raid10M", DynamicCeilingRaids)));
    DynamicFloorRaid10M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raid10M", DynamicFloorRaids)));
    DynamicCeilingRaid10MHeroic = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raid10MHeroic", DynamicCeilingHeroicRaids)));
    DynamicFloorRaid10MHeroic = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raid10MHeroic", DynamicFloorHeroicRaids)));

    DynamicCeilingRaid15M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raid15M", DynamicCeilingRaids)));
    DynamicFloorRaid15M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raid15M", DynamicFloorRaids)));

    DynamicCeilingRaid20M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raid20M", DynamicCeilingRaids)));
    DynamicFloorRaid20M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raid20M", DynamicFloorRaids)));

    DynamicCeilingRaid25M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raid25M", DynamicCeilingRaids)));
    DynamicFloorRaid25M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raid25M", DynamicFloorRaids)));
    DynamicCeilingRaid25MHeroic = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raid25MHeroic", DynamicCeilingHeroicRaids)));
    DynamicFloorRaid25MHeroic = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raid25MHeroic", DynamicFloorHeroicRaids)));

    DynamicCeilingRaid40M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raid40M", DynamicCeilingRaids)));
    DynamicFloorRaid40M = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raid40M", DynamicFloorRaids)));

    DynamicOverrides.clear();

    UseAutoBalanceSettings = sConfigMgr->GetOption<bool>("ItemScaling.UseAutoBalanceSettings", false);
    if (UseAutoBalanceSettings)
    {
        std::string abMethod = sConfigMgr->GetOption<std::string>("AutoBalance.LevelScaling.Method", "", false);
        if (abMethod == "fixed")
        {
            Method = SCALING_METHOD_FIXED;
        }
        else if (abMethod == "dynamic")
        {
            Method = SCALING_METHOD_DYNAMIC;
        }

        DynamicCeilingDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Ceiling.Dungeons", DynamicCeilingDungeons, false)));
        DynamicFloorDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Floor.Dungeons", DynamicFloorDungeons, false)));

        DynamicCeilingHeroicDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Ceiling.HeroicDungeons", DynamicCeilingHeroicDungeons, false)));
        DynamicFloorHeroicDungeons = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Floor.HeroicDungeons", DynamicFloorHeroicDungeons, false)));
        DynamicCeilingHeroicDungeonsTBC = DynamicCeilingHeroicDungeons;
        DynamicFloorHeroicDungeonsTBC = DynamicFloorHeroicDungeons;
        DynamicCeilingHeroicDungeonsWrath = DynamicCeilingHeroicDungeons;
        DynamicFloorHeroicDungeonsWrath = DynamicFloorHeroicDungeons;

        DynamicCeilingRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Ceiling.Raids", DynamicCeilingRaids, false)));
        DynamicFloorRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Floor.Raids", DynamicFloorRaids, false)));

        DynamicCeilingHeroicRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Ceiling.HeroicRaids", DynamicCeilingHeroicRaids, false)));
        DynamicFloorHeroicRaids = static_cast<uint8>(std::min<uint32>(80, sConfigMgr->GetOption<uint32>(
            "AutoBalance.LevelScaling.DynamicLevel.Floor.HeroicRaids", DynamicFloorHeroicRaids, false)));

        DynamicCeilingRaid10M = DynamicCeilingRaids;
        DynamicFloorRaid10M = DynamicFloorRaids;
        DynamicCeilingRaid10MHeroic = DynamicCeilingHeroicRaids;
        DynamicFloorRaid10MHeroic = DynamicFloorHeroicRaids;
        DynamicCeilingRaid15M = DynamicCeilingRaids;
        DynamicFloorRaid15M = DynamicFloorRaids;
        DynamicCeilingRaid20M = DynamicCeilingRaids;
        DynamicFloorRaid20M = DynamicFloorRaids;
        DynamicCeilingRaid25M = DynamicCeilingRaids;
        DynamicFloorRaid25M = DynamicFloorRaids;
        DynamicCeilingRaid25MHeroic = DynamicCeilingHeroicRaids;
        DynamicFloorRaid25MHeroic = DynamicFloorHeroicRaids;
        DynamicCeilingRaid40M = DynamicCeilingRaids;
        DynamicFloorRaid40M = DynamicFloorRaids;

        std::string abPerInstance = sConfigMgr->GetOption<std::string>(
            "AutoBalance.LevelScaling.DynamicLevel.PerInstance", "", false);
        if (!abPerInstance.empty())
        {
            ParseAutoBalanceDynamicOverrides(abPerInstance);
        }
    }

    std::string perInstanceConfig = sConfigMgr->GetOption<std::string>("ItemScaling.Dynamic.PerInstance", "");
    if (!perInstanceConfig.empty())
    {
        ParseItemScalingDynamicOverrides(perInstanceConfig);
    }

    if (!reload)
    {
        std::string syntheticStartStr = sConfigMgr->GetOption<std::string>("ItemScaling.SyntheticEntry.Start", "auto");
        if (syntheticStartStr == "auto" || syntheticStartStr == "0")
        {
            AutoSyntheticEntry = true;
            SyntheticEntryStart = 0;
        }
        else
        {
            AutoSyntheticEntry = false;
            if (Optional<uint32> val = Acore::StringTo<uint32>(syntheticStartStr))
            {
                SyntheticEntryStart = *val;
            }
            else
            {
                SyntheticEntryStart = 60000;
            }
        }

        SyntheticEntryAutoOffset = sConfigMgr->GetOption<uint32>("ItemScaling.SyntheticEntry.AutoOffset", 1000);
        DemandLedgerEnable = sConfigMgr->GetOption<bool>("ItemScaling.DemandLedger.Enable", true);
        MaxNewVariantsPerStartup = std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.MaxNewVariantsPerStartup", 25000), 1, 250000);
        SyntheticEntryMaximum = std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.SyntheticEntry.Maximum", 2000000), 60000, 10000000);
        FormulaVersion = static_cast<uint8>(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("ItemScaling.FormulaVersion", 1), 1, 255));
        LiveEnable = sConfigMgr->GetOption<bool>("ItemScaling.Live.Enable", true);
        LiveGenerationMode = static_cast<uint8>(std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.GenerationMode", 1), 1, 2));
        LiveReservedSlots = std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.ReservedSlots", 4096), 1, 250000);
        LiveMaxPendingVariants = std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.MaxPendingVariants", 4096), 1, 250000);
        LiveMaxPublishPerTick = std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.MaxPublishPerTick", 64), 1, 256);
        LiveLootWaitTimeoutMs = std::clamp<uint32>(
            sConfigMgr->GetOption<uint32>("ItemScaling.Live.LootWaitTimeoutMs", 10000), 1000, 60000);
    }

    BracketStep = static_cast<uint8>(std::clamp<uint32>(sConfigMgr->GetOption<uint32>("ItemScaling.BracketStep", 1), 1, 10));

    ExcludedLevels.clear();
    std::string excludedLevelsStr = sConfigMgr->GetOption<std::string>("ItemScaling.ExcludedLevels", "");
    for (std::string_view token : Acore::Tokenize(excludedLevelsStr, ',', false))
    {
        if (Optional<uint32> lvl = Acore::StringTo<uint32>(token))
        {
            if (*lvl >= 1 && *lvl <= 80)
            {
                ExcludedLevels.insert(static_cast<uint8>(*lvl));
            }
        }
    }

    ExcludedMapIds.clear();
    std::string excludedMaps = sConfigMgr->GetOption<std::string>("ItemScaling.ExcludedMapIds", "");
    for (std::string_view token : Acore::Tokenize(excludedMaps, ',', false))
    {
        if (Optional<uint32> mapId = Acore::StringTo<uint32>(token))
        {
            ExcludedMapIds.insert(*mapId);
        }
    }

    ExcludedItemIds.clear();
    std::string excludedItems = sConfigMgr->GetOption<std::string>("ItemScaling.ExcludedItemIds", "");
    for (std::string_view token : Acore::Tokenize(excludedItems, ',', false))
    {
        if (Optional<uint32> itemId = Acore::StringTo<uint32>(token))
        {
            ExcludedItemIds.insert(*itemId);
        }
    }

    Debug = sConfigMgr->GetOption<bool>("ItemScaling.Debug", false);
    Announce = sConfigMgr->GetOption<bool>("ItemScaling.Announce", true);
}

bool ItemScalingConfig::IsQualityEnabled(uint32 quality) const
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR:
            return ScalePoor;
        case ITEM_QUALITY_NORMAL:
            return ScaleCommon;
        case ITEM_QUALITY_UNCOMMON:
            return ScaleUncommon;
        case ITEM_QUALITY_RARE:
            return ScaleRare;
        case ITEM_QUALITY_EPIC:
            return ScaleEpic;
        case ITEM_QUALITY_LEGENDARY:
            return ScaleLegendary;
        case ITEM_QUALITY_ARTIFACT:
            return ScaleArtifact;
        case ITEM_QUALITY_HEIRLOOM:
            return ScaleHeirloom;
        default:
            return true;
    }
}

bool ItemScalingConfig::IsLevelExcluded(uint8 level) const
{
    return ExcludedLevels.find(level) != ExcludedLevels.end();
}

bool ItemScalingConfig::IsMapExcluded(uint32 mapId) const
{
    return ExcludedMapIds.find(mapId) != ExcludedMapIds.end();
}

bool ItemScalingConfig::IsItemExcluded(uint32 itemId) const
{
    return ExcludedItemIds.find(itemId) != ExcludedItemIds.end();
}

std::string ItemScalingConfig::GetInstanceCategoryDescription(Map const* map) const
{
    if (!map)
        return "[Dungeon]";

    if (map->IsRaid())
    {
        uint32 maxPlayers = map->GetMaxPlayers();
        bool isHeroic = map->IsHeroic();
        if (isHeroic)
        {
            if (maxPlayers <= 10)
                return "[10-man Heroic Raid]";
            if (maxPlayers <= 25)
                return "[25-man Heroic Raid]";
            return "[Heroic Raid]";
        }
        else
        {
            if (maxPlayers <= 10)
                return "[10-man Raid]";
            if (maxPlayers == 15)
                return "[15-man Raid]";
            if (maxPlayers == 20)
                return "[20-man Raid]";
            if (maxPlayers <= 25)
                return "[25-man Raid]";
            if (maxPlayers <= 40)
                return "[40-man Raid]";
            return "[Raid]";
        }
    }

    // Dungeon
    bool isHeroic = map->IsHeroic();
    uint32 expansion = map->GetEntry() ? map->GetEntry()->Expansion() : 0;
    if (isHeroic && expansion > 0)
    {
        if (expansion == 1)
            return "[TBC Heroic]";
        if (expansion == 2)
            return "[Wrath Heroic]";
        return "[Heroic Dungeon]";
    }

    return "[Dungeon]";
}

uint8 ItemScalingConfig::GetDynamicFloor(bool isRaid, bool isHeroic, uint32 expansion, uint32 maxPlayers, uint32 mapId) const
{
    if (mapId != 0)
    {
        auto it = DynamicOverrides.find(mapId);
        if (it != DynamicOverrides.end() && it->second.floor != -1)
            return static_cast<uint8>(it->second.floor);
    }

    if (isRaid)
    {
        if (UseAutoBalanceSettings)
        {
            return isHeroic ? DynamicFloorHeroicRaids : DynamicFloorRaids;
        }

        if (isHeroic)
        {
            if (maxPlayers != 0 && maxPlayers <= 10)
                return DynamicFloorRaid10MHeroic;
            if (maxPlayers != 0 && maxPlayers <= 25)
                return DynamicFloorRaid25MHeroic;
            return DynamicFloorHeroicRaids;
        }
        else
        {
            if (maxPlayers != 0)
            {
                if (maxPlayers <= 10)
                    return DynamicFloorRaid10M;
                if (maxPlayers == 15)
                    return DynamicFloorRaid15M;
                if (maxPlayers == 20)
                    return DynamicFloorRaid20M;
                if (maxPlayers <= 25)
                    return DynamicFloorRaid25M;
                if (maxPlayers <= 40)
                    return DynamicFloorRaid40M;
            }
            return DynamicFloorRaids;
        }
    }

    // Heroics are strictly supported for TBC (expansion 1) and Wrath (expansion 2).
    // Vanilla (expansion 0) does not have native heroics; fallback to standard dungeon rules.
    if (isHeroic && expansion > 0)
    {
        if (UseAutoBalanceSettings)
            return DynamicFloorHeroicDungeons;

        if (expansion == 1)
            return DynamicFloorHeroicDungeonsTBC;
        if (expansion == 2)
            return DynamicFloorHeroicDungeonsWrath;
        return DynamicFloorHeroicDungeons;
    }

    return DynamicFloorDungeons;
}

uint8 ItemScalingConfig::GetDynamicCeiling(bool isRaid, bool isHeroic, uint32 expansion, uint32 maxPlayers, uint32 mapId) const
{
    if (mapId != 0)
    {
        auto it = DynamicOverrides.find(mapId);
        if (it != DynamicOverrides.end() && it->second.ceiling != -1)
            return static_cast<uint8>(it->second.ceiling);
    }

    if (isRaid)
    {
        if (UseAutoBalanceSettings)
        {
            return isHeroic ? DynamicCeilingHeroicRaids : DynamicCeilingRaids;
        }

        if (isHeroic)
        {
            if (maxPlayers != 0 && maxPlayers <= 10)
                return DynamicCeilingRaid10MHeroic;
            if (maxPlayers != 0 && maxPlayers <= 25)
                return DynamicCeilingRaid25MHeroic;
            return DynamicCeilingHeroicRaids;
        }
        else
        {
            if (maxPlayers != 0)
            {
                if (maxPlayers <= 10)
                    return DynamicCeilingRaid10M;
                if (maxPlayers == 15)
                    return DynamicCeilingRaid15M;
                if (maxPlayers == 20)
                    return DynamicCeilingRaid20M;
                if (maxPlayers <= 25)
                    return DynamicCeilingRaid25M;
                if (maxPlayers <= 40)
                    return DynamicCeilingRaid40M;
            }
            return DynamicCeilingRaids;
        }
    }

    // Heroics are strictly supported for TBC (expansion 1) and Wrath (expansion 2).
    // Vanilla (expansion 0) does not have native heroics; fallback to standard dungeon rules.
    if (isHeroic && expansion > 0)
    {
        if (UseAutoBalanceSettings)
            return DynamicCeilingHeroicDungeons;

        if (expansion == 1)
            return DynamicCeilingHeroicDungeonsTBC;
        if (expansion == 2)
            return DynamicCeilingHeroicDungeonsWrath;
        return DynamicCeilingHeroicDungeons;
    }

    return DynamicCeilingDungeons;
}

uint8 ItemScalingConfig::GetDynamicFloor(Map const* map) const
{
    if (!map)
        return DynamicFloorDungeons;

    bool isRaid = map->IsRaid();
    bool isHeroic = map->IsHeroic();
    uint32 expansion = map->GetEntry() ? map->GetEntry()->Expansion() : 0;
    uint32 maxPlayers = map->GetMaxPlayers();
    uint32 mapId = map->GetId();
    return GetDynamicFloor(isRaid, isHeroic, expansion, maxPlayers, mapId);
}

uint8 ItemScalingConfig::GetDynamicCeiling(Map const* map) const
{
    if (!map)
        return DynamicCeilingDungeons;

    bool isRaid = map->IsRaid();
    bool isHeroic = map->IsHeroic();
    uint32 expansion = map->GetEntry() ? map->GetEntry()->Expansion() : 0;
    uint32 maxPlayers = map->GetMaxPlayers();
    uint32 mapId = map->GetId();
    return GetDynamicCeiling(isRaid, isHeroic, expansion, maxPlayers, mapId);
}

void ItemScalingConfig::ParseItemScalingDynamicOverrides(std::string const& configStr)
{
    if (configStr.empty())
        return;

    for (std::string_view entryView : Acore::Tokenize(configStr, ',', false))
    {
        std::vector<std::string_view> tokens;
        for (std::string_view tok : Acore::Tokenize(entryView, ' ', false))
        {
            if (!tok.empty())
                tokens.push_back(tok);
        }

        if (tokens.empty())
            continue;

        Optional<uint32> mapId = Acore::StringTo<uint32>(tokens[0]);
        if (!mapId)
            continue;

        int32 ceiling = -1;
        int32 floor = -1;

        if (tokens.size() >= 5)
        {
            if (Optional<int32> c = Acore::StringTo<int32>(tokens[3]))
                ceiling = *c;
            if (Optional<int32> f = Acore::StringTo<int32>(tokens[4]))
                floor = *f;
        }
        else if (tokens.size() >= 3)
        {
            if (Optional<int32> c = Acore::StringTo<int32>(tokens[1]))
                ceiling = *c;
            if (Optional<int32> f = Acore::StringTo<int32>(tokens[2]))
                floor = *f;
        }
        else if (tokens.size() == 2)
        {
            if (Optional<int32> c = Acore::StringTo<int32>(tokens[1]))
                ceiling = *c;
        }

        auto& overrideEntry = DynamicOverrides[*mapId];
        if (ceiling != -1)
            overrideEntry.ceiling = std::clamp<int32>(ceiling, 0, 80);
        if (floor != -1)
            overrideEntry.floor = std::clamp<int32>(floor, 0, 80);
    }
}

void ItemScalingConfig::ParseAutoBalanceDynamicOverrides(std::string const& configStr)
{
    if (configStr.empty())
        return;

    for (std::string_view entryView : Acore::Tokenize(configStr, ',', false))
    {
        std::vector<std::string_view> tokens;
        for (std::string_view tok : Acore::Tokenize(entryView, ' ', false))
        {
            if (!tok.empty())
                tokens.push_back(tok);
        }

        if (tokens.empty())
            continue;

        Optional<uint32> mapId = Acore::StringTo<uint32>(tokens[0]);
        if (!mapId)
            continue;

        int32 ceiling = -1;
        int32 floor = -1;

        if (tokens.size() >= 4)
        {
            if (Optional<int32> c = Acore::StringTo<int32>(tokens[3]))
                ceiling = *c;
        }
        if (tokens.size() >= 5)
        {
            if (Optional<int32> f = Acore::StringTo<int32>(tokens[4]))
                floor = *f;
        }

        auto& overrideEntry = DynamicOverrides[*mapId];
        if (ceiling != -1)
            overrideEntry.ceiling = std::clamp<int32>(ceiling, 0, 80);
        if (floor != -1)
            overrideEntry.floor = std::clamp<int32>(floor, 0, 80);
    }
}
