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
#include "Log.h"
#include "ObjectMgr.h"
#include "StringFormat.h"
#include "World.h"
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

ItemScalingRegistry* ItemScalingRegistry::instance()
{
    static ItemScalingRegistry instance;
    return &instance;
}

static std::string BuildItemTemplateInsertSQL(ItemTemplate const& scaledProto, uint32 baseEntry)
{
    std::string escapedName = scaledProto.Name1;
    WorldDatabase.EscapeString(escapedName);
    std::string nameExpr = escapedName.empty() ? "name" : ("'" + escapedName + "'");

    // Clone base row in item_template and override scaled fields
    return Acore::StringFormat(
        "INSERT INTO item_template ("
        "  entry, class, subclass, SoundOverrideSubclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, InventoryType, "
        "  AllowableClass, AllowableRace, ItemLevel, RequiredLevel, RequiredSkill, RequiredSkillRank, requiredspell, requiredhonorrank, "
        "  RequiredCityRank, RequiredReputationFaction, RequiredReputationRank, maxcount, stackable, ContainerSlots, "
        "  stat_type1, stat_value1, stat_type2, stat_value2, stat_type3, stat_value3, stat_type4, stat_value4, stat_type5, stat_value5, "
        "  stat_type6, stat_value6, stat_type7, stat_value7, stat_type8, stat_value8, stat_type9, stat_value9, stat_type10, stat_value10, "
        "  ScalingStatDistribution, ScalingStatValue, dmg_min1, dmg_max1, dmg_type1, dmg_min2, dmg_max2, dmg_type2, armor, "
        "  holy_res, fire_res, nature_res, frost_res, shadow_res, arcane_res, delay, ammo_type, RangedModRange, "
        "  spellid_1, spelltrigger_1, spellcharges_1, spellppmRate_1, spellcooldown_1, spellcategory_1, spellcategorycooldown_1, "
        "  spellid_2, spelltrigger_2, spellcharges_2, spellppmRate_2, spellcooldown_2, spellcategory_2, spellcategorycooldown_2, "
        "  spellid_3, spelltrigger_3, spellcharges_3, spellppmRate_3, spellcooldown_3, spellcategory_3, spellcategorycooldown_3, "
        "  spellid_4, spelltrigger_4, spellcharges_4, spellppmRate_4, spellcooldown_4, spellcategory_4, spellcategorycooldown_4, "
        "  spellid_5, spelltrigger_5, spellcharges_5, spellppmRate_5, spellcooldown_5, spellcategory_5, spellcategorycooldown_5, "
        "  bonding, description, PageText, LanguageID, PageMaterial, startquest, lockid, Material, sheath, RandomProperty, RandomSuffix, "
        "  block, itemset, MaxDurability, area, Map, BagFamily, TotemCategory, "
        "  socketColor_1, socketContent_1, socketColor_2, socketContent_2, socketColor_3, socketContent_3, socketBonus, "
        "  GemProperties, RequiredDisenchantSkill, ArmorDamageModifier, duration, ItemLimitCategory, HolidayId, ScriptName, DisenchantID, "
        "  FoodType, minMoneyLoot, maxMoneyLoot, flagsCustom"
        ") SELECT "
        "  {} AS entry, {} AS class, {} AS subclass, {} AS SoundOverrideSubclass, {} AS name, {} AS displayid, "
        "  Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, {} AS InventoryType, "
        "  AllowableClass, AllowableRace, {} AS ItemLevel, {} AS RequiredLevel, RequiredSkill, RequiredSkillRank, requiredspell, requiredhonorrank, "
        "  RequiredCityRank, RequiredReputationFaction, RequiredReputationRank, maxcount, stackable, ContainerSlots, "
        "  {} AS stat_type1, {} AS stat_value1, {} AS stat_type2, {} AS stat_value2, {} AS stat_type3, {} AS stat_value3, "
        "  {} AS stat_type4, {} AS stat_value4, {} AS stat_type5, {} AS stat_value5, {} AS stat_type6, {} AS stat_value6, "
        "  {} AS stat_type7, {} AS stat_value7, {} AS stat_type8, {} AS stat_value8, {} AS stat_type9, {} AS stat_value9, "
        "  {} AS stat_type10, {} AS stat_value10, "
        "  ScalingStatDistribution, ScalingStatValue, "
        "  {} AS dmg_min1, {} AS dmg_max1, dmg_type1, {} AS dmg_min2, {} AS dmg_max2, dmg_type2, "
        "  {} AS armor, {} AS holy_res, {} AS fire_res, {} AS nature_res, {} AS frost_res, "
        "  {} AS shadow_res, {} AS arcane_res, delay, ammo_type, RangedModRange, "
        "  spellid_1, spelltrigger_1, spellcharges_1, spellppmRate_1, spellcooldown_1, spellcategory_1, spellcategorycooldown_1, "
        "  spellid_2, spelltrigger_2, spellcharges_2, spellppmRate_2, spellcooldown_2, spellcategory_2, spellcategorycooldown_2, "
        "  spellid_3, spelltrigger_3, spellcharges_3, spellppmRate_3, spellcooldown_3, spellcategory_3, spellcategorycooldown_3, "
        "  spellid_4, spelltrigger_4, spellcharges_4, spellppmRate_4, spellcooldown_4, spellcategory_4, spellcategorycooldown_4, "
        "  spellid_5, spelltrigger_5, spellcharges_5, spellppmRate_5, spellcooldown_5, spellcategory_5, spellcategorycooldown_5, "
        "  bonding, description, PageText, LanguageID, PageMaterial, startquest, lockid, {} AS Material, {} AS sheath, {} AS RandomProperty, {} AS RandomSuffix, "
        "  {} AS block, itemset, MaxDurability, area, Map, BagFamily, TotemCategory, "
        "  socketColor_1, socketContent_1, socketColor_2, socketContent_2, socketColor_3, socketContent_3, socketBonus, "
        "  GemProperties, RequiredDisenchantSkill, ArmorDamageModifier, duration, ItemLimitCategory, HolidayId, ScriptName, DisenchantID, "
        "  FoodType, minMoneyLoot, maxMoneyLoot, flagsCustom "
        "FROM item_template WHERE entry = {};",
        scaledProto.ItemId, scaledProto.Class, scaledProto.SubClass, scaledProto.SoundOverrideSubclass,
        nameExpr, scaledProto.DisplayInfoID, scaledProto.InventoryType,
        scaledProto.ItemLevel,
        scaledProto.RequiredLevel,
        scaledProto.ItemStat[0].ItemStatType, scaledProto.ItemStat[0].ItemStatValue,
        scaledProto.ItemStat[1].ItemStatType, scaledProto.ItemStat[1].ItemStatValue,
        scaledProto.ItemStat[2].ItemStatType, scaledProto.ItemStat[2].ItemStatValue,
        scaledProto.ItemStat[3].ItemStatType, scaledProto.ItemStat[3].ItemStatValue,
        scaledProto.ItemStat[4].ItemStatType, scaledProto.ItemStat[4].ItemStatValue,
        scaledProto.ItemStat[5].ItemStatType, scaledProto.ItemStat[5].ItemStatValue,
        scaledProto.ItemStat[6].ItemStatType, scaledProto.ItemStat[6].ItemStatValue,
        scaledProto.ItemStat[7].ItemStatType, scaledProto.ItemStat[7].ItemStatValue,
        scaledProto.ItemStat[8].ItemStatType, scaledProto.ItemStat[8].ItemStatValue,
        scaledProto.ItemStat[9].ItemStatType, scaledProto.ItemStat[9].ItemStatValue,
        scaledProto.Damage[0].DamageMin, scaledProto.Damage[0].DamageMax,
        scaledProto.Damage[1].DamageMin, scaledProto.Damage[1].DamageMax,
        scaledProto.Armor,
        scaledProto.HolyRes, scaledProto.FireRes, scaledProto.NatureRes,
        scaledProto.FrostRes, scaledProto.ShadowRes, scaledProto.ArcaneRes,
        scaledProto.Material, scaledProto.Sheath, scaledProto.RandomProperty, scaledProto.RandomSuffix,
        scaledProto.Block,
        baseEntry
    );
}

namespace
{
    // All fields consumed by CreateScaledTemplate; other item attributes are cloned by INSERT ... SELECT.
    std::string const BaseColumns =
        "b.entry,b.class,b.subclass,b.Quality,b.InventoryType,b.ItemLevel,b.RequiredLevel,"
        "b.stat_type1,b.stat_value1,b.stat_type2,b.stat_value2,b.stat_type3,b.stat_value3,"
        "b.stat_type4,b.stat_value4,b.stat_type5,b.stat_value5,b.stat_type6,b.stat_value6,"
        "b.stat_type7,b.stat_value7,b.stat_type8,b.stat_value8,b.stat_type9,b.stat_value9,"
        "b.stat_type10,b.stat_value10,b.dmg_min1,b.dmg_max1,b.dmg_type1,b.dmg_min2,b.dmg_max2,"
        "b.dmg_type2,b.armor,b.delay,b.block,b.holy_res,b.fire_res,b.nature_res,b.frost_res,"
        "b.shadow_res,b.arcane_res,b.ScalingStatDistribution,b.ScalingStatValue,b.name,b.RandomProperty,b.RandomSuffix";

    ItemTemplate ReadBaseTemplate(Field* fields)
    {
        ItemTemplate item{};
        item.ItemId = fields[0].Get<uint32>();
        item.Class = fields[1].Get<uint8>();
        item.SubClass = fields[2].Get<uint8>();
        item.Quality = fields[3].Get<uint8>();
        item.InventoryType = fields[4].Get<uint8>();
        item.ItemLevel = fields[5].Get<uint16>();
        item.RequiredLevel = fields[6].Get<uint8>();
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            int32 value = fields[8 + i * 2].Get<int32>();
            if (!value)
                continue;
            auto& stat = item.ItemStat[item.StatsCount++];
            stat.ItemStatType = fields[7 + i * 2].Get<uint8>();
            stat.ItemStatValue = value;
        }
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            item.Damage[i].DamageMin = fields[27 + i * 3].Get<float>();
            item.Damage[i].DamageMax = fields[28 + i * 3].Get<float>();
            item.Damage[i].DamageType = fields[29 + i * 3].Get<uint8>();
        }
        item.Armor = fields[33].Get<uint32>();
        item.Delay = fields[34].Get<uint16>();
        item.Block = fields[35].Get<uint32>();
        item.HolyRes = fields[36].Get<int16>();
        item.FireRes = fields[37].Get<int16>();
        item.NatureRes = fields[38].Get<int16>();
        item.FrostRes = fields[39].Get<int16>();
        item.ShadowRes = fields[40].Get<int16>();
        item.ArcaneRes = fields[41].Get<int16>();
        item.ScalingStatDistribution = fields[42].Get<uint16>();
        item.ScalingStatValue = fields[43].Get<uint32>();
        item.Name1 = fields[44].Get<std::string>();
        item.RandomProperty = fields[45].Get<int32>();
        item.RandomSuffix = fields[46].Get<int32>();
        return item;
    }

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

    std::string KeyPredicate(VariantKey const& key)
    {
        return Acore::StringFormat(
            "base_entry={} AND target_effective_level={} AND target_item_level={} AND formula_version={} "
            "AND generator_revision={} AND required_level={} AND random_property_id={}", key.baseEntry, key.targetEffectiveLevel,
            key.targetItemLevel, key.formulaVersion, key.generatorRevision, key.requiredLevel, key.randomPropertyId);
    }

    std::string DeleteRequest(VariantKey const& key)
    {
        return "DELETE FROM scaled_item_variant_request WHERE " + KeyPredicate(key);
    }

    std::string DeleteCompletedRequest(VariantKey const& key, uint32 entry)
    {
        return Acore::StringFormat(
            "DELETE FROM scaled_item_variant_request WHERE {} "
            "AND EXISTS (SELECT 1 FROM scaled_item_variant WHERE variant_entry={})", KeyPredicate(key), entry);
    }

    std::string VariantInsert(uint32 entry, VariantKey const& key, ItemScalingIdentity const& identity)
    {
        return Acore::StringFormat(
            "INSERT INTO scaled_item_variant "
            "(variant_entry,base_entry,target_effective_level,target_item_level,formula_version,"
            "generator_revision,required_level,random_property_id,base_class,base_subclass,base_sound_override_subclass,"
            "base_material,base_displayid,base_inventory_type,base_sheath,preserve_nonzero_stats) "
            "SELECT {},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{} FROM item_template WHERE entry={};",
            entry, key.baseEntry, key.targetEffectiveLevel, key.targetItemLevel, key.formulaVersion,
            key.generatorRevision, key.requiredLevel, key.randomPropertyId, identity.itemClass, identity.subClass,
            identity.soundOverrideSubclass, identity.material, identity.displayId, identity.inventoryType,
            identity.sheath, uint32(sItemScalingConfig->PreserveNonZeroStats), entry);
    }

    // DirectCommitTransaction has no success return. Verify each committed batch before proceeding.
    bool CommitBatch(WorldDatabaseTransaction& transaction, std::vector<uint32>& entries, uint32& uncommittedDeletions)
    {
        if (entries.empty() && uncommittedDeletions == 0)
            return true;
        WorldDatabase.DirectCommitTransaction(transaction);
        if (!entries.empty())
        {
            std::string ids;
            for (uint32 entry : entries)
            {
                if (!ids.empty())
                    ids += ',';
                ids += std::to_string(entry);
            }
            QueryResult result = WorldDatabase.Query(
                "SELECT COUNT(*) FROM scaled_item_variant s "
                "JOIN item_template i ON i.entry=s.variant_entry "
                "WHERE s.variant_entry IN ({}) AND i.ItemLevel=s.target_item_level "
                "AND i.RequiredLevel=s.required_level "
                "AND i.class=s.base_class AND i.subclass=s.base_subclass "
                "AND i.SoundOverrideSubclass=s.base_sound_override_subclass AND i.Material=s.base_material "
                "AND i.displayid=s.base_displayid AND i.InventoryType=s.base_inventory_type AND i.sheath=s.base_sheath "
                "AND NOT EXISTS (SELECT 1 FROM scaled_item_variant_request r WHERE r.base_entry=s.base_entry "
                "AND r.target_effective_level=s.target_effective_level AND r.target_item_level=s.target_item_level "
                "AND r.formula_version=s.formula_version AND r.generator_revision=s.generator_revision "
                "AND r.required_level=s.required_level AND r.random_property_id=s.random_property_id)", ids);
            if (!result || result->Fetch()[0].Get<uint64>() != entries.size())
            {
                LOG_ERROR("module.ItemScaling", "ItemScaling startup transaction verification failed; scaling disabled.");
                return false;
            }
            entries.clear();
        }
        uncommittedDeletions = 0;
        transaction = WorldDatabase.BeginTransaction();
        return true;
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
    for (std::string const table : {"scaled_item_variant", "scaled_item_variant_request"})
    {
        bool const request = table == "scaled_item_variant_request";
        std::unordered_map<std::string, ColumnType> expected = {
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
            {"preserve_nonzero_stats", {"tinyint", true, false}}
        };
        if (request)
            expected.emplace("requested_at", ColumnType{"timestamp", false, false});
        else
        {
            expected.emplace("variant_entry", ColumnType{"int", true, false});
            expected.emplace("created_at", ColumnType{"timestamp", false, false});
        }

        QueryResult columns = WorldDatabase.Query(
            "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE FROM information_schema.COLUMNS "
            "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='{}'", table);
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
                    LOG_ERROR("module.ItemScaling", "Incompatible {}.{}; install the complete module schema.",
                        table, it->first);
                    return false;
                }
                expected.erase(it);
            } while (columns->NextRow());
        }
        if (!expected.empty())
        {
            LOG_ERROR("module.ItemScaling", "Missing {} columns; install the complete module schema.", table);
            return false;
        }

        QueryResult index = WorldDatabase.Query(
            "SELECT COLUMN_NAME,NON_UNIQUE FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
            "AND TABLE_NAME='{}' AND INDEX_NAME='{}' ORDER BY SEQ_IN_INDEX",
            table, request ? "PRIMARY" : "uk_variant_key");
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
            LOG_ERROR("module.ItemScaling", "Incompatible {} identity key; "
                "install the complete module schema.", table);
            return false;
        }
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
        "AND TABLE_NAME IN ('item_template','scaled_item_variant','scaled_item_variant_request') AND ENGINE='InnoDB'");
    if (!engines || engines->Fetch()[0].Get<uint64>() != 3)
    {
        LOG_ERROR("module.ItemScaling", "ItemScaling requires InnoDB for all three transactional tables.");
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

bool ItemScalingRegistry::MaterializePendingRequests()
{
    if (!sItemScalingConfig->DemandLedgerEnable)
    {
        LOG_INFO("server.loading", "ItemScaling: demand ledger disabled; pending requests retained.");
        return true;
    }
    QueryResult count = WorldDatabase.Query("SELECT COUNT(*) FROM scaled_item_variant_request");
    if (!count)
        return false;
    if (!count->Fetch()[0].Get<uint64>())
    {
        LOG_INFO("server.loading", "ItemScaling: 0 pending requests; no new variants generated.");
        return true;
    }

    QueryResult result = WorldDatabase.Query(
        "SELECT {},{},preserve_nonzero_stats FROM scaled_item_variant_request "
        "ORDER BY requested_at,{}", KeyColumns, IdentityColumns, KeyColumns);
    if (!result)
        return false;
    uint64 const found = result->GetRowCount();
    uint32 created = 0;
    uint32 redundant = 0;
    uint32 stale = 0;
    uint32 deferred = 0;
    std::unordered_set<VariantKey, VariantKeyHash> existingVariantKeys;
    QueryResult existingKeys = WorldDatabase.Query("SELECT {} FROM scaled_item_variant", KeyColumns);
    if (existingKeys)
    {
        existingVariantKeys.reserve(static_cast<std::size_t>(existingKeys->GetRowCount()));
        do
        {
            existingVariantKeys.insert(ReadKey(existingKeys->Fetch()));
        } while (existingKeys->NextRow());
    }

    std::unordered_map<uint32, std::optional<ItemTemplate>> baseCache;
    uint32 baseCacheHits = 0;
    uint32 uncommittedDeletions = 0;

    auto transaction = WorldDatabase.BeginTransaction();
    std::vector<uint32> entries;
    do
    {
        Field* fields = result->Fetch();
        VariantKey key = ReadKey(fields);
        if (existingVariantKeys.find(key) != existingVariantKeys.end())
        {
            // A durable mapping owns this identity even if its template is missing or invalid.
            transaction->Append(DeleteRequest(key));
            ++uncommittedDeletions;
            ++redundant;
            if (uncommittedDeletions >= 250 && !CommitBatch(transaction, entries, uncommittedDeletions))
                return false;
            continue;
        }
        if (!ValidKey(key) || key.generatorRevision != ITEM_SCALING_GENERATOR_REVISION ||
            key.formulaVersion != sItemScalingConfig->FormulaVersion ||
            fields[14].Get<uint8>() != uint8(sItemScalingConfig->PreserveNonZeroStats))
        {
            transaction->Append(DeleteRequest(key));
            ++uncommittedDeletions;
            ++stale;
            if (uncommittedDeletions >= 250 && !CommitBatch(transaction, entries, uncommittedDeletions))
                return false;
            continue;
        }
        if (!_familyCompatible || created >= sItemScalingConfig->MaxNewVariantsPerStartup ||
            _nextSyntheticEntry > sItemScalingConfig->SyntheticEntryMaximum)
        {
            ++deferred;
            continue;
        }
        auto cacheIt = baseCache.find(key.baseEntry);
        if (cacheIt == baseCache.end())
        {
            QueryResult row = WorldDatabase.Query(
                "SELECT {} FROM item_template b WHERE b.entry={} "
                "AND NOT EXISTS (SELECT 1 FROM scaled_item_variant s WHERE s.variant_entry=b.entry)",
                BaseColumns, key.baseEntry);
            if (!row)
            {
                cacheIt = baseCache.emplace(key.baseEntry, std::nullopt).first;
            }
            else
            {
                cacheIt = baseCache.emplace(key.baseEntry, ReadBaseTemplate(row->Fetch())).first;
            }
        }
        else
        {
            ++baseCacheHits;
        }

        if (!cacheIt->second.has_value())
        {
            ++deferred;
            continue;
        }
        ItemTemplate const& base = *cacheIt->second;
        if (!ItemScalingFormula::IsScalableEquipment(&base))
        {
            ++deferred;
            continue;
        }
        ItemScalingIdentity const identity = ReadIdentity(fields + 7);
        uint32 entry = static_cast<uint32>(_nextSyntheticEntry++);
        ItemTemplate scaled = ItemScalingFormula::CreateScaledTemplate(&base, entry,
            key.targetEffectiveLevel, key.targetItemLevel, key.formulaVersion, key.requiredLevel, key.randomPropertyId);
        scaled.RequiredLevel = key.requiredLevel;
        identity.Apply(scaled);
        transaction->Append(BuildItemTemplateInsertSQL(scaled, key.baseEntry));
        transaction->Append(VariantInsert(entry, key, identity));
        transaction->Append(DeleteCompletedRequest(key, entry));
        entries.push_back(entry);
        existingVariantKeys.insert(key);
        ++created;
        if ((entries.size() + uncommittedDeletions) >= 250 && !CommitBatch(transaction, entries, uncommittedDeletions))
            return false;
    } while (result->NextRow());
    if (!CommitBatch(transaction, entries, uncommittedDeletions))
        return false;
    LOG_INFO("server.loading", "ItemScaling: pending {}, materialized {}, redundant removed {}, stale retired {}, "
        "deferred {} (missing/ineligible base or startup/ID limit), base template cache hits {}.",
        found, created, redundant, stale, deferred, baseCacheHits);
    if (stale)
        LOG_WARN("module.ItemScaling", "Retired {} stale/invalid requests; gameplay may request the active family.", stale);
    if (deferred)
        LOG_WARN("module.ItemScaling", "{} requests remain pending; original items continue to drop.", deferred);
    return true;
}

void ItemScalingRegistry::OnLoadCustomDatabaseTable()
{
    if (_dbSynchronized)
        return;
    if (!ValidateSchema() || !ResolveSyntheticEntryRange() ||
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
    _dbSynchronized = MaterializePendingRequests();
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
            _requestedKeys.insert(key);
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
            if (!ItemScalingIdentity::CompatibleLootMetadata(*base, *persisted))
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

uint32 ItemScalingRegistry::FindOrRequestVariant(ItemTemplate const* baseProto, uint8 targetEffectiveLevel,
    uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId)
{
    if (!baseProto || !_initialized.load(std::memory_order_acquire) || _syntheticEntries.count(baseProto->ItemId))
        return 0;
    uint8 required = ItemScalingFormula::CalculateRequiredLevel(baseProto, targetEffectiveLevel, highestRealPlayerLevel);
    VariantKey key{baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion,
        ITEM_SCALING_GENERATOR_REVISION, required, randomPropertyId};
    auto it = _keyToEntry.find(key);
    if (it != _keyToEntry.end())
        return it->second;
    if (_familyCompatible && sItemScalingConfig->DemandLedgerEnable && ValidKey(key) &&
        ItemScalingIdentity::CanCapture(*baseProto))
        QueueVariantRequest(key, *baseProto);
    return 0;
}

void ItemScalingRegistry::QueueVariantRequest(VariantKey const& key, ItemTemplate const& base)
{
    {
        std::lock_guard<std::mutex> lock(_requestMutex);
        if (!_requestedKeys.insert(key).second)
            return;
    }
    ItemScalingIdentity const identity = ItemScalingIdentity::Capture(base);
    // Numeric fields only. Execute copies/enqueues the SQL; no DB round trip or ObjectMgr write on map workers.
    WorldDatabase.Execute(
        "INSERT IGNORE INTO scaled_item_variant_request "
        "(base_entry,target_effective_level,target_item_level,formula_version,generator_revision,required_level,random_property_id,"
        "base_class,base_subclass,base_sound_override_subclass,base_material,base_displayid,base_inventory_type,"
        "base_sheath,preserve_nonzero_stats) VALUES ({},{},{},{},{},{},{},{},{},{},{},{},{},{},{})",
        key.baseEntry, key.targetEffectiveLevel, key.targetItemLevel, key.formulaVersion, key.generatorRevision,
        key.requiredLevel, key.randomPropertyId, identity.itemClass, identity.subClass, identity.soundOverrideSubclass, identity.material,
        identity.displayId, identity.inventoryType, identity.sheath, uint32(sItemScalingConfig->PreserveNonZeroStats));
    if (sItemScalingConfig->Debug)
        LOG_INFO("module.ItemScaling", "Queued demand for base {}, target {}, item level {}, required {}.",
            key.baseEntry, key.targetEffectiveLevel, key.targetItemLevel, key.requiredLevel);
}
