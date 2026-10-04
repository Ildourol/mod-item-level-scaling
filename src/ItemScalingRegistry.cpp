/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingRegistry.h"
#include "ItemScalingSafety.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingIdentity.h"
#include "ItemScalingLive.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "StringFormat.h"
#include "World.h"
#include <set>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

ItemScalingRegistry* ItemScalingRegistry::instance()
{
    static ItemScalingRegistry instance;
    return &instance;
}

namespace
{
    VariantKey ReadKey(Field* fields)
    {
        return {fields[0].Get<uint32>(), fields[1].Get<uint8>(), fields[2].Get<uint16>(),
            fields[3].Get<uint8>(), fields[4].Get<uint8>(), fields[5].Get<uint8>(), fields[6].Get<int32>()};
    }

    bool ValidKey(VariantKey const& key)
    {
        return key.baseEntry != 0 && key.targetEffectiveLevel >= 1 && key.targetEffectiveLevel <= 80 &&
            key.targetItemLevel >= 1 && key.targetItemLevel <= 300 && key.formulaVersion != 0 &&
            key.generatorRevision != 0 && key.requiredLevel >= 1 && key.requiredLevel <= 80;
    }

    std::string const KeyColumns =
        "base_entry,target_effective_level,target_item_level,formula_version,generator_revision,required_level,random_property_id";
    std::string const IdentityColumns =
        "base_class,base_subclass,base_sound_override_subclass,base_material,base_displayid,"
        "base_inventory_type,base_sheath";

    ItemScalingIdentity ReadIdentity(Field* fields)
    {
        return {fields[0].Get<uint8>(), fields[1].Get<uint8>(), fields[2].Get<int8>(),
            fields[3].Get<int8>(), fields[4].Get<uint32>(), fields[5].Get<uint8>(), fields[6].Get<uint8>()};
    }
}

void ItemScalingRegistry::EnsureSchema()
{
    // Idempotently create tables if they do not exist
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `scaled_item_variant` ("
        "  `variant_entry` INT UNSIGNED NOT NULL,"
        "  `base_entry` INT UNSIGNED NOT NULL,"
        "  `target_effective_level` TINYINT UNSIGNED NOT NULL,"
        "  `target_item_level` SMALLINT UNSIGNED NOT NULL,"
        "  `formula_version` TINYINT UNSIGNED NOT NULL,"
        "  `generator_revision` TINYINT UNSIGNED NOT NULL,"
        "  `required_level` TINYINT UNSIGNED NOT NULL,"
        "  `random_property_id` INT NOT NULL DEFAULT 0,"
        "  `base_class` TINYINT UNSIGNED NOT NULL,"
        "  `base_subclass` TINYINT UNSIGNED NOT NULL,"
        "  `base_sound_override_subclass` TINYINT NOT NULL,"
        "  `base_material` TINYINT NOT NULL,"
        "  `base_displayid` INT UNSIGNED NOT NULL,"
        "  `base_inventory_type` TINYINT UNSIGNED NOT NULL,"
        "  `base_sheath` TINYINT UNSIGNED NOT NULL,"
        "  `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL,"
        "  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`variant_entry`),"
        "  UNIQUE KEY `uk_variant_key` (`base_entry`, `target_effective_level`, `target_item_level`, `formula_version`, `generator_revision`, `required_level`, `random_property_id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;"
    );

    // MySQL does not support ADD COLUMN IF NOT EXISTS. Inspect once during startup,
    // then apply only missing columns with portable ALTER syntax.
    QueryResult columns = WorldDatabase.Query(
        "SELECT TABLE_NAME,COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant'");
    if (!columns)
        return;
    std::set<std::pair<std::string, std::string>> present;
    do
    {
        Field* fields = columns->Fetch();
        present.emplace(fields[0].Get<std::string>(), fields[1].Get<std::string>());
    } while (columns->NextRow());
    struct Migration
    {
        char const* table;
        char const* column;
        char const* sql;
    };
    static Migration const migrations[] = {
        {"scaled_item_variant", "generator_revision", "ALTER TABLE `scaled_item_variant` ADD COLUMN `generator_revision` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `formula_version`;"},
        {"scaled_item_variant", "required_level", "ALTER TABLE `scaled_item_variant` ADD COLUMN `required_level` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `generator_revision`;"},
        {"scaled_item_variant", "random_property_id", "ALTER TABLE `scaled_item_variant` ADD COLUMN `random_property_id` INT NOT NULL DEFAULT 0 AFTER `required_level`;"},
        {"scaled_item_variant", "base_class", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_class` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `random_property_id`;"},
        {"scaled_item_variant", "base_subclass", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_subclass` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_class`;"},
        {"scaled_item_variant", "base_sound_override_subclass", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_sound_override_subclass` TINYINT NOT NULL DEFAULT -1 AFTER `base_subclass`;"},
        {"scaled_item_variant", "base_material", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_material` TINYINT NOT NULL DEFAULT 0 AFTER `base_sound_override_subclass`;"},
        {"scaled_item_variant", "base_displayid", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_displayid` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_material`;"},
        {"scaled_item_variant", "base_inventory_type", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_inventory_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_displayid`;"},
        {"scaled_item_variant", "base_sheath", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_sheath` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_inventory_type`;"},
        {"scaled_item_variant", "preserve_nonzero_stats", "ALTER TABLE `scaled_item_variant` ADD COLUMN `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `base_sheath`;"},
    };
    for (auto const& migration : migrations)
        if (!present.count({migration.table, migration.column}))
            WorldDatabase.DirectExecute(migration.sql);

    QueryResult uk = WorldDatabase.Query(
        "SELECT COUNT(*),COALESCE(SUM(COLUMN_NAME='random_property_id'),0) FROM information_schema.STATISTICS "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key'");
    if (uk && uk->Fetch()[1].Get<uint64>() == 0)
    {
        if (uk->Fetch()[0].Get<uint64>() != 0)
            WorldDatabase.DirectExecute("ALTER TABLE `scaled_item_variant` DROP KEY `uk_variant_key`");
        WorldDatabase.DirectExecute(
            "ALTER TABLE `scaled_item_variant` ADD UNIQUE KEY `uk_variant_key` ("
            "`base_entry`,`target_effective_level`,`target_item_level`,`formula_version`,`generator_revision`,`required_level`,`random_property_id`)"
        );
    }
}

bool ItemScalingRegistry::ValidateSchema()
{
    struct ColumnType
    {
        std::string type;
        bool isUnsigned;
        bool nullable;
    };
    std::unordered_map<std::string, ColumnType> expected = {
        {"variant_entry", {"int", true, false}},
        {"base_entry", {"int", true, false}},
        {"target_effective_level", {"tinyint", true, false}},
        {"target_item_level", {"smallint", true, false}},
        {"formula_version", {"tinyint", true, false}},
        {"generator_revision", {"tinyint", true, false}},
        {"required_level", {"tinyint", true, false}},
        {"random_property_id", {"int", false, false}},
        {"base_class", {"tinyint", true, false}},
        {"base_subclass", {"tinyint", true, false}},
        {"base_sound_override_subclass", {"tinyint", false, false}},
        {"base_material", {"tinyint", false, false}},
        {"base_displayid", {"int", true, false}},
        {"base_inventory_type", {"tinyint", true, false}},
        {"base_sheath", {"tinyint", true, false}},
        {"preserve_nonzero_stats", {"tinyint", true, false}},
        {"created_at", {"timestamp", false, false}}
    };

    QueryResult columns = WorldDatabase.Query(
        "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant'");
    if (columns)
    {
        do
        {
            Field* field = columns->Fetch();
            auto it = expected.find(field[0].Get<std::string>());
            if (it == expected.end())
                continue;
            ColumnType const& type = it->second;
            if (field[1].Get<std::string>() != type.type ||
                (field[2].Get<std::string>().find("unsigned") != std::string::npos) != type.isUnsigned ||
                (field[3].Get<std::string>() == "YES") != type.nullable)
            {
                LOG_ERROR("module.ItemScaling", "Incompatible scaled_item_variant.{}; install the complete module schema.",
                    it->first);
                return false;
            }
            expected.erase(it);
        } while (columns->NextRow());
    }
    if (!expected.empty())
    {
        LOG_ERROR("module.ItemScaling", "Missing scaled_item_variant columns; install the complete module schema.");
        return false;
    }

    QueryResult index = WorldDatabase.Query(
        "SELECT COLUMN_NAME,NON_UNIQUE FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key' ORDER BY SEQ_IN_INDEX");
    std::vector<std::string> indexColumns;
    if (index)
    {
        do
        {
            if (index->Fetch()[1].Get<uint8>() != 0)
                return false;
            indexColumns.push_back(index->Fetch()[0].Get<std::string>());
        } while (index->NextRow());
    }
    std::vector<std::string> const keyColumns = {"base_entry", "target_effective_level", "target_item_level",
        "formula_version", "generator_revision", "required_level", "random_property_id"};
    if (indexColumns != keyColumns)
    {
        LOG_ERROR("module.ItemScaling", "Incompatible scaled_item_variant identity key; "
            "install the complete module schema.");
        return false;
    }
    QueryResult primary = WorldDatabase.Query(
        "SELECT COLUMN_NAME FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='PRIMARY' ORDER BY SEQ_IN_INDEX");
    if (!primary || primary->GetRowCount() != 1 || primary->Fetch()[0].Get<std::string>() != "variant_entry")
    {
        LOG_ERROR("module.ItemScaling", "Invalid committed variant primary key; "
            "install the complete module schema.");
        return false;
    }
    QueryResult engines = WorldDatabase.Query(
        "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME IN ('item_template','scaled_item_variant') AND ENGINE='InnoDB'");
    if (!engines || engines->Fetch()[0].Get<uint64>() != 2)
    {
        LOG_ERROR("module.ItemScaling", "ItemScaling requires InnoDB for both transactional tables.");
        return false;
    }
    return true;
}

bool ItemScalingRegistry::ResolveSyntheticEntryRange()
{
    QueryResult result = WorldDatabase.Query(
        "SELECT COALESCE((SELECT MAX(entry) FROM item_template),0),"
        "COALESCE((SELECT MAX(variant_entry) FROM scaled_item_variant),0)");
    if (!result)
        return false;
    uint64 highestItem = result->Fetch()[0].Get<uint64>();
    uint64 highestVariant = result->Fetch()[1].Get<uint64>();
    _nextSyntheticEntry = ItemScalingSafety::AllocationStart(highestItem, highestVariant,
        sItemScalingConfig->AutoSyntheticEntry, sItemScalingConfig->SyntheticEntryStart,
        sItemScalingConfig->SyntheticEntryAutoOffset);
    // Do not overwrite configuration with allocator state. Existing IDs are never renumbered.
    if (_nextSyntheticEntry > sItemScalingConfig->SyntheticEntryMaximum)
    {
        LOG_WARN("module.ItemScaling", "Synthetic ID limit reached; pending requests will be retained.");
    }
    LOG_INFO("server.loading",
        "ItemScaling: synthetic IDs highest item {}, highest variant {}, next {}, maximum {}.",
        highestItem, highestVariant, _nextSyntheticEntry, sItemScalingConfig->SyntheticEntryMaximum);
    return true;
}

void ItemScalingRegistry::OnLoadCustomDatabaseTable()
{
    if (_dbSynchronized)
        return;
    EnsureSchema();
    bool validSchema = ValidateSchema();
    if (!sItemScalingLive->RecoverStagedTemplates())
    {
        LOG_ERROR("module.ItemScaling", "Live snapshot recovery failed; stopping startup to protect issued items.");
        World::StopNow(ERROR_EXIT_CODE);
        return;
    }
    if (!validSchema)
        return;
    if (!sItemScalingConfig->Enable)
    {
        // Recovery still runs when disabled so existing character items remain valid.
        if (!sItemScalingLive->ReserveSlots())
        {
            LOG_ERROR("module.ItemScaling", "Live slot ownership failed validation; stopping startup.");
            World::StopNow(ERROR_EXIT_CODE);
        }
        return;
    }
    if (!ResolveSyntheticEntryRange() ||
        !ItemScalingFormula::LoadStartupCurves(sWorld->GetDataPath()))
    {
        LOG_ERROR("module.ItemScaling", "Startup prerequisites failed; item scaling disabled for this run.");
        return;
    }
    LOG_INFO("server.loading",
        "ItemScaling: formula version {}, generator revision {}, target levels {}-{}, bracket {}.",
        sItemScalingConfig->FormulaVersion, ITEM_SCALING_GENERATOR_REVISION,
        sItemScalingConfig->MinLevel, sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep);
    QueryResult family = WorldDatabase.Query(
        "SELECT COUNT(*) FROM scaled_item_variant WHERE generator_revision={} AND formula_version={} "
        "AND preserve_nonzero_stats<>{}", ITEM_SCALING_GENERATOR_REVISION,
        sItemScalingConfig->FormulaVersion, uint32(sItemScalingConfig->PreserveNonZeroStats));
    if (!family)
        return;
    _familyCompatible = family->Fetch()[0].Get<uint64>() == 0;
    if (!_familyCompatible)
        LOG_WARN("module.ItemScaling", "PreserveNonZeroStats conflicts with the committed FormulaVersion family. "
            "Increment FormulaVersion before generating new variants; existing issued items remain available.");
    if (!sItemScalingLive->ReserveSlots())
    {
        LOG_ERROR("module.ItemScaling", "Could not reserve live slots; stopping startup before player login.");
        _dbSynchronized = false;
        World::StopNow(ERROR_EXIT_CODE);
        return;
    }
    _dbSynchronized = true;
}

void ItemScalingRegistry::Initialize()
{
    if (!_dbSynchronized || _initialized.load(std::memory_order_acquire))
        return;

    QueryResult count = WorldDatabase.Query("SELECT COUNT(*) FROM scaled_item_variant");
    if (!count)
        return;
    QueryResult result = WorldDatabase.Query(
        "SELECT variant_entry,{},{},preserve_nonzero_stats FROM scaled_item_variant", KeyColumns, IdentityColumns);
    if (count->Fetch()[0].Get<uint64>() != (result ? result->GetRowCount() : 0))
    {
        LOG_ERROR("module.ItemScaling", "Could not read the complete committed registry; scaling disabled.");
        return;
    }
    uint32 validated = 0;
    uint32 inactiveFamily = 0;
    uint32 missingTemplate = 0;
    uint32 invalid = 0;
    uint32 incompatible = 0;
    if (result)
    {
        _keyToEntry.reserve(static_cast<std::size_t>(result->GetRowCount()));
        _syntheticEntries.reserve(static_cast<std::size_t>(result->GetRowCount()));
        do
        {
            Field* fields = result->Fetch();
            uint32 entry = fields[0].Get<uint32>();
            VariantKey key = ReadKey(fields + 1);
            _syntheticEntries.insert(entry);
            // Do not enqueue another request for a committed but unusable identity.
            _committedKeys.insert(key);
            ItemTemplate const* persisted = sObjectMgr->GetItemTemplate(entry);
            ItemTemplate const* base = sObjectMgr->GetItemTemplate(key.baseEntry);
            if (!ValidKey(key) || !entry || entry == key.baseEntry || !base ||
                !ItemScalingIdentity::CanCapture(*base))
            {
                ++invalid;
                continue;
            }
            if (!persisted)
            {
                ++missingTemplate;
                continue;
            }
            ItemScalingIdentity const identity = ReadIdentity(fields + 8);
            if (!identity.Matches(*base) || !identity.Matches(*persisted) || fields[15].Get<uint8>() > 1)
            {
                ++invalid;
                continue;
            }
            if (persisted->RequiredLevel != key.requiredLevel || persisted->ItemLevel != key.targetItemLevel)
            {
                ++invalid;
                continue;
            }
            if (!ItemScalingIdentity::CompatibleLootMetadata(*base, *persisted, key.randomPropertyId != 0))
            {
                ++incompatible;
                continue;
            }
            ++validated;
            if (key.generatorRevision == ITEM_SCALING_GENERATOR_REVISION &&
                key.formulaVersion == sItemScalingConfig->FormulaVersion &&
                fields[15].Get<uint8>() == uint8(sItemScalingConfig->PreserveNonZeroStats))
                _keyToEntry.emplace(key, entry);
            else
                ++inactiveFamily;
        } while (result->NextRow());
    }
    // Build statistical baseline from in-memory ItemTemplateStore, bypassing SQL table scan
    sItemScalingLive->IncludeOwnedEntries(_syntheticEntries);
    sItemScalingBaseline->BuildBaseline(&_syntheticEntries);
    // Publish immutable lookup tables only after all startup writes to them have finished.
    _initialized.store(true, std::memory_order_release);
    LOG_INFO("server.loading", "ItemScaling: validated {}, indexed {}, inactive family {}, missing templates {}, "
        "invalid records {}, incompatible loot metadata {}.", validated, _keyToEntry.size(), inactiveFamily,
        missingTemplate, invalid, incompatible);
    if (missingTemplate)
        LOG_WARN("module.ItemScaling", "{} committed variants withheld: item_template rows are missing. "
            "Restore the exact rows from a consistent backup; their keys and IDs remain reserved.", missingTemplate);
    if (invalid || incompatible)
        LOG_WARN("module.ItemScaling", "{} committed variants withheld: key, identity, levels or inherited loot "
            "metadata failed validation. Review the data; committed rows were not changed.", invalid + incompatible);
}

uint32 ItemScalingRegistry::FindExistingVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel,
    uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId) const
{
    if (!baseProto || !_initialized.load(std::memory_order_acquire) || _syntheticEntries.count(baseProto->ItemId))
        return 0;
    uint8 required = ItemScalingFormula::CalculateRequiredLevel(baseProto, targetEffectiveLevel, highestRealPlayerLevel);
    VariantKey key{baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion,
        ITEM_SCALING_GENERATOR_REVISION, required, randomPropertyId};
    auto it = _keyToEntry.find(key);
    if (it != _keyToEntry.end())
        return it->second;
    return sItemScalingLive->Find(key);
}

uint32 ItemScalingRegistry::FindOrRequestVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel,
    uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId)
{
    if (!baseProto || !_initialized.load(std::memory_order_acquire) || _syntheticEntries.count(baseProto->ItemId))
        return 0;
    if (sItemScalingConfig->PreserveNativeLoot &&
        ItemScalingFormula::IsNativeTargetMatch(baseProto, targetEffectiveLevel, targetEffectiveLevel, highestRealPlayerLevel))
        return 0;
    uint8 required = ItemScalingFormula::CalculateRequiredLevel(baseProto, targetEffectiveLevel, highestRealPlayerLevel);
    VariantKey key{baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion,
        ITEM_SCALING_GENERATOR_REVISION, required, randomPropertyId};
    auto it = _keyToEntry.find(key);
    if (it != _keyToEntry.end())
        return it->second;
    if (_familyCompatible && sItemScalingConfig->LiveEnable && ValidKey(key) &&
        ItemScalingIdentity::CanCapture(*baseProto))
    {
        // A committed but invalid key remains reserved. Never stage another ID for
        // it: that would collide with its permanent unique key during recovery.
        if (_committedKeys.count(key))
            return 0;

        return sItemScalingLive->FindOrRequest(*baseProto, key, highestRealPlayerLevel);
    }
    return 0;
}
