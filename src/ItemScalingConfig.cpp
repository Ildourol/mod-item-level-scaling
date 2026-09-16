/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingConfig.h"
#include "Tokenize.h"
#include "StringConvert.h"
#include <string_view>

ItemScalingConfig* ItemScalingConfig::instance()
{
    static ItemScalingConfig instance;
    return &instance;
}

void ItemScalingConfig::Load()
{
    Enable = sConfigMgr->GetOption<bool>("ItemScaling.Enable", true);
    ScaleDungeons = sConfigMgr->GetOption<bool>("ItemScaling.ScaleDungeons", true);
    ScaleRaids = sConfigMgr->GetOption<bool>("ItemScaling.ScaleRaids", true);
    ScaleHeroics = sConfigMgr->GetOption<bool>("ItemScaling.ScaleHeroics", true);
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

    MinLevel = static_cast<uint8>(sConfigMgr->GetOption<uint32>("ItemScaling.MinLevel", 1));
    MaxLevel = static_cast<uint8>(sConfigMgr->GetOption<uint32>("ItemScaling.MaxLevel", 80));
    if (MinLevel < 1)
    {
        MinLevel = 1;
    }
    if (MaxLevel > 80)
    {
        MaxLevel = 80;
    }
    if (MinLevel > MaxLevel)
    {
        std::swap(MinLevel, MaxLevel);
    }

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

    PreserveNonZeroStats = sConfigMgr->GetOption<bool>("ItemScaling.PreserveNonZeroStats", true);

    std::string reqPolicyStr = sConfigMgr->GetOption<std::string>("ItemScaling.RequiredLevel.Policy", "target-capped-player");
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

    DynamicFloorDungeons = static_cast<uint8>(sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Dungeons", 5));
    DynamicCeilingDungeons = static_cast<uint8>(sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Dungeons", 3));
    DynamicFloorRaids = static_cast<uint8>(sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Floor.Raids", 5));
    DynamicCeilingRaids = static_cast<uint8>(sConfigMgr->GetOption<uint32>("ItemScaling.Dynamic.Ceiling.Raids", 3));

    UseAutoBalanceSettings = sConfigMgr->GetOption<bool>("ItemScaling.UseAutoBalanceSettings", true);
    SyntheticEntryStart = sConfigMgr->GetOption<uint32>("ItemScaling.SyntheticEntry.Start", 10000000);
    FormulaVersion = static_cast<uint8>(sConfigMgr->GetOption<uint32>("ItemScaling.FormulaVersion", 1));

    ExcludedLevels.clear();
    std::string excludedLevelsStr = sConfigMgr->GetOption<std::string>("ItemScaling.ExcludedLevels", "60, 70, 80");
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

uint8 ItemScalingConfig::GetDynamicFloor(bool isRaid) const
{
    return isRaid ? DynamicFloorRaids : DynamicFloorDungeons;
}

uint8 ItemScalingConfig::GetDynamicCeiling(bool isRaid) const
{
    return isRaid ? DynamicCeilingRaids : DynamicCeilingDungeons;
}
