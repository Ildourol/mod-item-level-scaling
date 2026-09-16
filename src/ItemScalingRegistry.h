/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_REGISTRY_H
#define _ITEM_SCALING_REGISTRY_H

#include "ItemScalingCommon.h"
#include <atomic>
#include <map>
#include <shared_mutex>

class ItemScalingRegistry
{
public:
    static ItemScalingRegistry* instance();

    void Initialize();

    [[nodiscard]] uint32 GetOrCreateVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel);

private:
    std::shared_mutex _cacheLock;
    std::map<VariantKey, uint32> _keyToEntry;
    std::atomic<uint32> _nextSyntheticEntry{10000000};
    bool _initialized{false};
};

#define sItemScalingRegistry ItemScalingRegistry::instance()

#endif // _ITEM_SCALING_REGISTRY_H
