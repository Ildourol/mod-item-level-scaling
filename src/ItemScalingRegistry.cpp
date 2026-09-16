/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingRegistry.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "Log.h"
#include "ObjectMgr.h"

ItemScalingRegistry* ItemScalingRegistry::instance()
{
    static ItemScalingRegistry instance;
    return &instance;
}

void ItemScalingRegistry::Initialize()
{
    if (_initialized)
    {
        return;
    }

    _nextSyntheticEntry.store(sItemScalingConfig->SyntheticEntryStart);

    // 1. Ensure persistence table exists
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `scaled_item_variant` ("
        "  `variant_entry` INT UNSIGNED NOT NULL,"
        "  `base_entry` INT UNSIGNED NOT NULL,"
        "  `target_effective_level` TINYINT UNSIGNED NOT NULL,"
        "  `target_item_level` SMALLINT UNSIGNED NOT NULL,"
        "  `formula_version` TINYINT UNSIGNED NOT NULL DEFAULT 1,"
        "  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`variant_entry`),"
        "  UNIQUE KEY `uk_variant_key` (`base_entry`, `target_effective_level`, `target_item_level`, `formula_version`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Persisted scaled item variants';"
    );

    // 2. Load all persisted variants
    QueryResult result = WorldDatabase.Query(
        "SELECT variant_entry, base_entry, target_effective_level, target_item_level, formula_version "
        "FROM scaled_item_variant ORDER BY variant_entry ASC"
    );

    uint32 loadedCount = 0;
    uint32 maxVariantEntry = sItemScalingConfig->SyntheticEntryStart;

    if (result)
    {
        std::unique_lock lock(_cacheLock);

        do
        {
            Field* fields = result->Fetch();
            uint32 variantEntry = fields[0].Get<uint32>();
            uint32 baseEntry = fields[1].Get<uint32>();
            uint8 targetEffectiveLevel = fields[2].Get<uint8>();
            uint16 targetItemLevel = fields[3].Get<uint16>();
            uint8 formulaVersion = fields[4].Get<uint8>();

            if (variantEntry > maxVariantEntry)
            {
                maxVariantEntry = variantEntry;
            }

            ItemTemplate const* baseProto = sObjectMgr->GetItemTemplate(baseEntry);
            if (!baseProto)
            {
                LOG_WARN("module.ItemScaling", "ItemScalingRegistry::Initialize: Base item {} for variant {} not found in item_template store!", baseEntry, variantEntry);
                continue;
            }

            // Reconstruct the synthetic ItemTemplate deterministically
            ItemTemplate scaledProto = ItemScalingFormula::CreateScaledTemplate(
                baseProto,
                variantEntry,
                targetEffectiveLevel,
                targetItemLevel,
                formulaVersion,
                targetEffectiveLevel
            );

            // Register into ObjectMgr
            sObjectMgr->AddCustomItemTemplate(scaledProto);

            VariantKey key;
            key.baseEntry = baseEntry;
            key.targetEffectiveLevel = targetEffectiveLevel;
            key.targetItemLevel = targetItemLevel;
            key.formulaVersion = formulaVersion;

            _keyToEntry[key] = variantEntry;
            ++loadedCount;
        } while (result->NextRow());
    }

    _nextSyntheticEntry.store(std::max(maxVariantEntry + 1, sItemScalingConfig->SyntheticEntryStart));
    _initialized = true;

    LOG_INFO("server.loading", ">> ItemScaling: Loaded and registered {} persisted scaled item variants (next synthetic ID: {}).", loadedCount, _nextSyntheticEntry.load());
}

uint32 ItemScalingRegistry::GetOrCreateVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel)
{
    if (!baseProto)
    {
        return 0;
    }

    VariantKey key;
    key.baseEntry = baseProto->ItemId;
    key.targetEffectiveLevel = targetEffectiveLevel;
    key.targetItemLevel = targetItemLevel;
    key.formulaVersion = formulaVersion;

    // Fast-path read with shared_lock
    {
        std::shared_lock lock(_cacheLock);
        auto itr = _keyToEntry.find(key);
        if (itr != _keyToEntry.end())
        {
            return itr->second;
        }
    }

    // Slow-path insertion with unique_lock
    std::unique_lock lock(_cacheLock);

    // Double-check after acquiring write lock
    auto itr = _keyToEntry.find(key);
    if (itr != _keyToEntry.end())
    {
        return itr->second;
    }

    // Allocate collision-safe unique synthetic ID
    uint32 newEntry = 0;
    while (true)
    {
        uint32 candidate = _nextSyntheticEntry.fetch_add(1);
        if (!sObjectMgr->GetItemTemplate(candidate))
        {
            newEntry = candidate;
            break;
        }
    }

    // Construct scaled template
    ItemTemplate scaledProto = ItemScalingFormula::CreateScaledTemplate(
        baseProto,
        newEntry,
        targetEffectiveLevel,
        targetItemLevel,
        formulaVersion,
        highestRealPlayerLevel
    );

    // Persist to database
    WorldDatabase.Execute(
        "INSERT IGNORE INTO scaled_item_variant (variant_entry, base_entry, target_effective_level, target_item_level, formula_version) "
        "VALUES ({}, {}, {}, {}, {})",
        newEntry,
        baseProto->ItemId,
        targetEffectiveLevel,
        targetItemLevel,
        formulaVersion
    );

    // Register into ObjectMgr custom store
    sObjectMgr->AddCustomItemTemplate(scaledProto);

    _keyToEntry[key] = newEntry;

    if (sItemScalingConfig->Debug)
    {
        LOG_INFO("module.ItemScaling", "ItemScalingRegistry: Created scaled variant {} for base item {} '{}' (Target Lvl: {}, Target Ilvl: {})",
            newEntry, baseProto->ItemId, baseProto->Name1, targetEffectiveLevel, targetItemLevel);
    }

    return newEntry;
}
