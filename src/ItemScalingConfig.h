/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_CONFIG_H
#define _ITEM_SCALING_CONFIG_H

#include "Config.h"
#include "ItemScalingCommon.h"
#include <unordered_set>

class ItemScalingConfig
{
public:
    static ItemScalingConfig* instance();

    void Load();

    bool Enable{true};
    bool ScaleDungeons{true};
    bool ScaleRaids{true};
    bool ScaleHeroics{true};
    bool ScaleChests{true};
    ItemScalingMethod Method{SCALING_METHOD_DYNAMIC};
    uint8 MinLevel{1};
    uint8 MaxLevel{80};
    bool RealPlayersOnly{true};
    bool IncludeGameMasters{true};
    bool ScaleUp{true};
    bool ScaleDown{true};
    bool ScaleExistingScalingItems{false};

    bool ScalePoor{true};
    bool ScaleCommon{true};
    bool ScaleUncommon{true};
    bool ScaleRare{true};
    bool ScaleEpic{true};
    bool ScaleLegendary{true};
    bool ScaleArtifact{true};
    bool ScaleHeirloom{false};

    bool PreserveNonZeroStats{true};
    RequiredLevelPolicy ReqLevelPolicy{REQ_POLICY_TARGET_CAPPED_PLAYER};

    uint8 DynamicFloorDungeons{5};
    uint8 DynamicCeilingDungeons{3};
    uint8 DynamicFloorRaids{5};
    uint8 DynamicCeilingRaids{3};

    bool UseAutoBalanceSettings{false};
    bool AutoSyntheticEntry{true};
    uint32 SyntheticEntryStart{60000};
    uint32 SyntheticEntryAutoOffset{1000};
    bool PreStageDungeonLoot{true};
    uint32 MaxNewVariantsPerStartup{25000};
    uint32 SyntheticEntryMaximum{2000000};
    uint8 BracketStep{2};
    uint8 FormulaVersion{1};

    std::unordered_set<uint8> ExcludedLevels;
    std::unordered_set<uint32> ExcludedMapIds;
    std::unordered_set<uint32> ExcludedItemIds;

    bool Debug{false};

    [[nodiscard]] bool IsQualityEnabled(uint32 quality) const;
    [[nodiscard]] bool IsLevelExcluded(uint8 level) const;
    [[nodiscard]] bool IsMapExcluded(uint32 mapId) const;
    [[nodiscard]] bool IsItemExcluded(uint32 itemId) const;
    [[nodiscard]] uint8 GetDynamicFloor(bool isRaid) const;
    [[nodiscard]] uint8 GetDynamicCeiling(bool isRaid) const;
};

#define sItemScalingConfig ItemScalingConfig::instance()

#endif // _ITEM_SCALING_CONFIG_H
