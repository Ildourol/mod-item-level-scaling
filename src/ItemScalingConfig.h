/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_CONFIG_H
#define _ITEM_SCALING_CONFIG_H

#include "Config.h"
#include "ItemScalingCommon.h"
#include <unordered_set>
#include <unordered_map>
#include <string>

class Map;

class ItemScalingConfig
{
public:
    static ItemScalingConfig* instance();

    void Load(bool reload = false);

    bool Enable{true};
    bool ScaleDungeons{true};
    bool ScaleRaids{true};
    bool ScaleHeroics{true};
    bool ScaleHeroicDungeons{true};
    bool ScaleRaid10M{true};
    bool ScaleRaid10MHeroic{true};
    bool ScaleRaid15M{true};
    bool ScaleRaid20M{true};
    bool ScaleRaid25M{true};
    bool ScaleRaid25MHeroic{true};
    bool ScaleRaid40M{true};
    bool ScaleChests{true};
    ItemScalingMethod Method{SCALING_METHOD_DYNAMIC};
    uint8 MinLevel{1};
    uint8 MaxLevel{80};
    bool RealPlayersOnly{true};
    bool IncludeGameMasters{false};
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

    RandomSuffixScalingMode RandomSuffixMode{RandomSuffixScalingMode::Skip};

    bool PreserveNonZeroStats{true};
    RequiredLevelPolicy ReqLevelPolicy{REQ_POLICY_TARGET};

    uint8 DynamicFloorDungeons{3};
    uint8 DynamicCeilingDungeons{5};
    uint8 DynamicFloorRaids{0};
    uint8 DynamicCeilingRaids{3};

    uint8 DynamicFloorHeroicDungeons{0};
    uint8 DynamicCeilingHeroicDungeons{5};
    uint8 DynamicFloorHeroicDungeonsTBC{0};
    uint8 DynamicCeilingHeroicDungeonsTBC{5};
    uint8 DynamicFloorHeroicDungeonsWrath{0};
    uint8 DynamicCeilingHeroicDungeonsWrath{5};

    uint8 DynamicFloorHeroicRaids{0};
    uint8 DynamicCeilingHeroicRaids{3};

    uint8 DynamicFloorRaid10M{0};
    uint8 DynamicCeilingRaid10M{3};
    uint8 DynamicFloorRaid10MHeroic{0};
    uint8 DynamicCeilingRaid10MHeroic{3};

    uint8 DynamicFloorRaid15M{0};
    uint8 DynamicCeilingRaid15M{3};

    uint8 DynamicFloorRaid20M{0};
    uint8 DynamicCeilingRaid20M{3};

    uint8 DynamicFloorRaid25M{0};
    uint8 DynamicCeilingRaid25M{3};
    uint8 DynamicFloorRaid25MHeroic{0};
    uint8 DynamicCeilingRaid25MHeroic{3};

    uint8 DynamicFloorRaid40M{0};
    uint8 DynamicCeilingRaid40M{3};

    struct DynamicLevelOverride
    {
        int32 ceiling{-1};
        int32 floor{-1};
    };
    std::unordered_map<uint32, DynamicLevelOverride> DynamicOverrides;

    bool UseAutoBalanceSettings{false};
    bool AutoSyntheticEntry{true};
    uint32 SyntheticEntryStart{60000};
    uint32 SyntheticEntryAutoOffset{1000};
    bool DemandLedgerEnable{true};
    uint32 MaxNewVariantsPerStartup{25000};
    uint32 SyntheticEntryMaximum{2000000};
    uint8 BracketStep{1};
    uint8 FormulaVersion{1};

    // Startup-only: live templates use slots already loaded by ObjectMgr.
    bool LiveEnable{true};
    uint8 LiveGenerationMode{1};
    uint32 LiveReservedSlots{4096};
    uint32 LiveMaxPendingVariants{4096};
    uint32 LiveMaxPublishPerTick{64};
    uint32 LiveLootWaitTimeoutMs{10000};
    uint32 Revision{0};

    std::unordered_set<uint8> ExcludedLevels;
    std::unordered_set<uint32> ExcludedMapIds;
    std::unordered_set<uint32> ExcludedItemIds;

    bool Debug{false};
    bool Announce{true};

    [[nodiscard]] bool IsQualityEnabled(uint32 quality) const;
    [[nodiscard]] bool IsLevelExcluded(uint8 level) const;
    [[nodiscard]] bool IsMapExcluded(uint32 mapId) const;
    [[nodiscard]] bool IsItemExcluded(uint32 itemId) const;
    [[nodiscard]] std::string GetInstanceCategoryDescription(Map const* map) const;
    [[nodiscard]] uint8 GetDynamicFloor(bool isRaid, bool isHeroic = false, uint32 expansion = 0, uint32 maxPlayers = 0, uint32 mapId = 0) const;
    [[nodiscard]] uint8 GetDynamicCeiling(bool isRaid, bool isHeroic = false, uint32 expansion = 0, uint32 maxPlayers = 0, uint32 mapId = 0) const;
    [[nodiscard]] uint8 GetDynamicFloor(Map const* map) const;
    [[nodiscard]] uint8 GetDynamicCeiling(Map const* map) const;

    void ParseItemScalingDynamicOverrides(std::string const& configStr);
    void ParseAutoBalanceDynamicOverrides(std::string const& configStr);
};

#define sItemScalingConfig ItemScalingConfig::instance()

#endif // _ITEM_SCALING_CONFIG_H
