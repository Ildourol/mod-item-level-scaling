/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_REGISTRY_H
#define _ITEM_SCALING_REGISTRY_H

#include "ItemScalingCommon.h"
#include <atomic>
#include <map>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

inline uint64 PackVariantKey(uint32 baseEntry, uint8 targetLevel, uint16 targetIlvl, uint8 formulaVersion)
{
    return (static_cast<uint64>(baseEntry) << 32) |
           (static_cast<uint64>(targetLevel) << 24) |
           (static_cast<uint64>(targetIlvl) << 8) |
           static_cast<uint64>(formulaVersion);
}

class ItemScalingRegistry
{
public:
    static ItemScalingRegistry* instance();

    // Invoked during WorldScript::OnLoadCustomDatabaseTable() before ObjectMgr::LoadItemTemplates()
    void OnLoadCustomDatabaseTable();

    // Invoked during WorldScript::OnStartup() after all tables and DBCs are loaded
    void Initialize();

    // Fast O(1) in-memory lookup during loot generation (zero DB writes, zero lag)
    [[nodiscard]] uint32 GetVariantEntry(uint32 baseEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion) const;
    [[nodiscard]] inline uint32 GetVariantEntryFast(uint32 baseEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion) const
    {
        return GetVariantEntry(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion);
    }

private:
    void ResolveSyntheticEntryRange();
    void SynchronizeExistingVariants();
    void PreStageDungeonLoot();

    struct VariantRecord
    {
        uint32 variantEntry{0};
        uint8 targetEffectiveLevel{0};
        uint16 targetItemLevel{0};
    };

    mutable std::shared_mutex _cacheLock;
    std::unordered_map<uint64, uint32> _keyToEntry;
    std::unordered_map<uint32, std::vector<VariantRecord>> _baseToVariants;
    std::atomic<uint32> _nextSyntheticEntry{60000};
    bool _dbSynchronized{false};
    bool _initialized{false};
};

#define sItemScalingRegistry ItemScalingRegistry::instance()

#endif // _ITEM_SCALING_REGISTRY_H
