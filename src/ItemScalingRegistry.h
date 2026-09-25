/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_REGISTRY_H
#define _ITEM_SCALING_REGISTRY_H

#include "ItemScalingCommon.h"
#include <atomic>
#include <unordered_map>

class ItemScalingRegistry
{
public:
    static ItemScalingRegistry* instance();

    // Generate durable rows before the core loads item_template.
    void OnLoadCustomDatabaseTable();

    // Re-publish current variants from validated base templates before the world becomes connectable.
    void Initialize();

    // Gameplay is lookup-only. A miss leaves the original loot unchanged.
    [[nodiscard]] uint32 FindVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel,
        uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel) const;

private:
    bool ValidateSchema();
    bool ResolveSyntheticEntryRange();
    bool SynchronizeExistingVariants();
    bool PreStageDungeonLoot();

    std::unordered_map<VariantKey, uint32, VariantKeyHash> _keyToEntry;
    uint64 _nextSyntheticEntry{0};
    bool _dbSynchronized{false};
    std::atomic<bool> _initialized{false};
};

#define sItemScalingRegistry ItemScalingRegistry::instance()

#endif
