/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_REGISTRY_H
#define _ITEM_SCALING_REGISTRY_H

#include "ItemScalingCommon.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class ItemScalingRegistry
{
public:
    static ItemScalingRegistry* instance();

    // Generate durable rows before the core loads item_template.
    void OnLoadCustomDatabaseTable();

    // Validate and index core-loaded templates before gameplay can select variants.
    void Initialize();

    // Hits select loaded templates; misses queue exact demand and leave the original loot unchanged.
    [[nodiscard]] uint32 FindOrRequestVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel,
        uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel);

private:
    bool ValidateSchema();
    bool ResolveSyntheticEntryRange();
    bool MaterializePendingRequests();
    void QueueVariantRequest(VariantKey const& key, ItemTemplate const& base);

    std::unordered_map<VariantKey, uint32, VariantKeyHash> _keyToEntry;
    std::unordered_set<uint32> _syntheticEntries;
    std::mutex _requestMutex;
    std::unordered_set<VariantKey, VariantKeyHash> _requestedKeys;
    uint64 _nextSyntheticEntry{0};
    bool _familyCompatible{true};
    bool _dbSynchronized{false};
    std::atomic<bool> _initialized{false};
};

#define sItemScalingRegistry ItemScalingRegistry::instance()

#endif
