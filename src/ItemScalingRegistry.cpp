/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#include "ItemScalingRegistry.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "StringFormat.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

ItemScalingRegistry* ItemScalingRegistry::instance()
{
    static ItemScalingRegistry instance;
    return &instance;
}

void ItemScalingRegistry::ResolveSyntheticEntryRange()
{
    if (_dbSynchronized)
    {
        return;
    }

    uint32 startEntry = sItemScalingConfig->SyntheticEntryStart;

    if (sItemScalingConfig->AutoSyntheticEntry)
    {
        QueryResult resMax = WorldDatabase.Query("SELECT COALESCE(MAX(entry), 54806) FROM item_template WHERE entry < 1000000 AND entry NOT IN (SELECT variant_entry FROM scaled_item_variant)");
        uint32 dbMax = 54806;
        if (resMax)
        {
            dbMax = resMax->Fetch()[0].Get<uint32>();
        }

        // Check if scaled_item_variant already has an existing allocated range
        QueryResult resVar = WorldDatabase.Query("SELECT COALESCE(MIN(variant_entry), 0), COALESCE(MAX(variant_entry), 0) FROM scaled_item_variant WHERE variant_entry < 1000000");
        uint32 varMin = 0;
        uint32 varMax = 0;
        if (resVar)
        {
            Field* f = resVar->Fetch();
            varMin = f[0].Get<uint32>();
            varMax = f[1].Get<uint32>();
        }

        if (varMin > 0)
        {
            startEntry = varMin;
            _nextSyntheticEntry.store(std::max(varMax + 1, dbMax + 1));
        }
        else
        {
            uint32 candidate = dbMax + sItemScalingConfig->SyntheticEntryAutoOffset;
            if (candidate < 60000)
            {
                candidate = 60000;
            }
            else
            {
                candidate = ((candidate + 999) / 1000) * 1000;
            }
            startEntry = candidate;
            _nextSyntheticEntry.store(startEntry);
        }

        sItemScalingConfig->SyntheticEntryStart = startEntry;
        LOG_INFO("server.loading", ">> ItemScaling: Auto-detected compact synthetic entry range starting at {} (DB max item: {}).", startEntry, dbMax);
    }
    else
    {
        QueryResult resMax = WorldDatabase.Query("SELECT COALESCE(MAX(variant_entry), {}) FROM scaled_item_variant", startEntry);
        uint32 maxVar = startEntry;
        if (resMax)
        {
            maxVar = resMax->Fetch()[0].Get<uint32>();
        }
        _nextSyntheticEntry.store(std::max(maxVar + 1, startEntry));
    }
}

static std::string BuildItemTemplateInsertSQL(ItemTemplate const& scaledProto, uint32 baseEntry)
{
    // Clone base row in item_template and override scaled fields
    return Acore::StringFormat(
        "INSERT IGNORE INTO item_template ("
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
        "  {} AS entry, class, subclass, SoundOverrideSubclass, name, displayid, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, SellPrice, InventoryType, "
        "  AllowableClass, AllowableRace, {} AS ItemLevel, {} AS RequiredLevel, RequiredSkill, RequiredSkillRank, requiredspell, requiredhonorrank, "
        "  RequiredCityRank, RequiredReputationFaction, RequiredReputationRank, maxcount, stackable, ContainerSlots, "
        "  {} AS stat_type1, {} AS stat_value1, {} AS stat_type2, {} AS stat_value2, {} AS stat_type3, {} AS stat_value3, "
        "  {} AS stat_type4, {} AS stat_value4, {} AS stat_type5, {} AS stat_value5, {} AS stat_type6, {} AS stat_value6, "
        "  {} AS stat_type7, {} AS stat_value7, {} AS stat_type8, {} AS stat_value8, {} AS stat_type9, {} AS stat_value9, "
        "  {} AS stat_type10, {} AS stat_value10, "
        "  ScalingStatDistribution, ScalingStatValue, "
        "  {} AS dmg_min1, {} AS dmg_max1, dmg_type1, {} AS dmg_min2, {} AS dmg_max2, dmg_type2, "
        "  {} AS armor, holy_res, fire_res, nature_res, frost_res, shadow_res, arcane_res, delay, ammo_type, RangedModRange, "
        "  spellid_1, spelltrigger_1, spellcharges_1, spellppmRate_1, spellcooldown_1, spellcategory_1, spellcategorycooldown_1, "
        "  spellid_2, spelltrigger_2, spellcharges_2, spellppmRate_2, spellcooldown_2, spellcategory_2, spellcategorycooldown_2, "
        "  spellid_3, spelltrigger_3, spellcharges_3, spellppmRate_3, spellcooldown_3, spellcategory_3, spellcategorycooldown_3, "
        "  spellid_4, spelltrigger_4, spellcharges_4, spellppmRate_4, spellcooldown_4, spellcategory_4, spellcategorycooldown_4, "
        "  spellid_5, spelltrigger_5, spellcharges_5, spellppmRate_5, spellcooldown_5, spellcategory_5, spellcategorycooldown_5, "
        "  bonding, description, PageText, LanguageID, PageMaterial, startquest, lockid, Material, sheath, RandomProperty, RandomSuffix, "
        "  block, itemset, MaxDurability, area, Map, BagFamily, TotemCategory, "
        "  socketColor_1, socketContent_1, socketColor_2, socketContent_2, socketColor_3, socketContent_3, socketBonus, "
        "  GemProperties, RequiredDisenchantSkill, ArmorDamageModifier, duration, ItemLimitCategory, HolidayId, ScriptName, DisenchantID, "
        "  FoodType, minMoneyLoot, maxMoneyLoot, flagsCustom "
        "FROM item_template WHERE entry = {};",
        scaledProto.ItemId,
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
        baseEntry
    );
}

static std::string BuildVariantInsertSQL(uint32 variantEntry, uint32 baseEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion)
{
    return Acore::StringFormat(
        "INSERT IGNORE INTO scaled_item_variant (variant_entry, base_entry, target_effective_level, target_item_level, formula_version) "
        "VALUES ({}, {}, {}, {}, {});",
        variantEntry, baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion
    );
}

void ItemScalingRegistry::SynchronizeExistingVariants()
{
    // Find all variants in scaled_item_variant that are missing from item_template using a single consolidated JOIN query
    QueryResult result = WorldDatabase.Query(
        "SELECT s.variant_entry, s.base_entry, s.target_effective_level, s.target_item_level, s.formula_version, "
        "       b.class, b.subclass, b.Quality, b.InventoryType, b.ItemLevel, b.RequiredLevel, "
        "       b.stat_type1, b.stat_value1, b.stat_type2, b.stat_value2, b.stat_type3, b.stat_value3, "
        "       b.stat_type4, b.stat_value4, b.stat_type5, b.stat_value5, b.stat_type6, b.stat_value6, "
        "       b.stat_type7, b.stat_value7, b.stat_type8, b.stat_value8, b.stat_type9, b.stat_value9, "
        "       b.stat_type10, b.stat_value10, b.dmg_min1, b.dmg_max1, b.dmg_type1, b.dmg_min2, "
        "       b.dmg_max2, b.dmg_type2, b.armor, b.delay "
        "FROM scaled_item_variant s "
        "JOIN item_template b ON s.base_entry = b.entry "
        "LEFT JOIN item_template i ON s.variant_entry = i.entry "
        "WHERE i.entry IS NULL"
    );

    if (!result)
    {
        return;
    }

    WorldDatabaseTransaction trans = WorldDatabase.BeginTransaction();
    uint32 batchOps = 0;
    uint32 restoredCount = 0;

    do
    {
        Field* fields = result->Fetch();
        uint32 variantEntry = fields[0].Get<uint32>();
        uint32 baseEntry = fields[1].Get<uint32>();
        uint8 targetEffectiveLevel = fields[2].Get<uint8>();
        uint16 targetItemLevel = fields[3].Get<uint16>();
        uint8 formulaVersion = fields[4].Get<uint8>();

        ItemTemplate baseProto;
        baseProto.ItemId = baseEntry;
        baseProto.Class = fields[5].Get<uint32>();
        baseProto.SubClass = fields[6].Get<uint32>();
        baseProto.Quality = fields[7].Get<uint32>();
        baseProto.InventoryType = fields[8].Get<uint32>();
        baseProto.ItemLevel = fields[9].Get<uint32>();
        baseProto.RequiredLevel = fields[10].Get<uint32>();

        uint32 statCount = 0;
        for (uint32 s = 0; s < 10; ++s)
        {
            baseProto.ItemStat[s].ItemStatType = fields[11 + (s * 2)].Get<uint32>();
            baseProto.ItemStat[s].ItemStatValue = fields[12 + (s * 2)].Get<int32>();
            if (baseProto.ItemStat[s].ItemStatType != 0 || baseProto.ItemStat[s].ItemStatValue != 0)
            {
                ++statCount;
            }
        }
        baseProto.StatsCount = statCount;

        baseProto.Damage[0].DamageMin = fields[31].Get<float>();
        baseProto.Damage[0].DamageMax = fields[32].Get<float>();
        baseProto.Damage[0].DamageType = fields[33].Get<uint32>();
        baseProto.Damage[1].DamageMin = fields[34].Get<float>();
        baseProto.Damage[1].DamageMax = fields[35].Get<float>();
        baseProto.Damage[1].DamageType = fields[36].Get<uint32>();
        baseProto.Armor = fields[37].Get<uint32>();
        baseProto.Delay = fields[38].Get<uint32>();

        ItemTemplate scaledProto = ItemScalingFormula::CreateScaledTemplate(
            &baseProto,
            variantEntry,
            targetEffectiveLevel,
            targetItemLevel,
            formulaVersion,
            targetEffectiveLevel
        );

        trans->Append(BuildItemTemplateInsertSQL(scaledProto, baseEntry));
        ++batchOps;
        ++restoredCount;

        if (batchOps >= 500)
        {
            WorldDatabase.DirectCommitTransaction(trans);
            trans = WorldDatabase.BeginTransaction();
            batchOps = 0;
        }
    } while (result->NextRow());

    if (batchOps > 0)
    {
        WorldDatabase.DirectCommitTransaction(trans);
    }

    if (restoredCount > 0)
    {
        LOG_INFO("server.loading", ">> ItemScaling: Synchronized {} missing scaled item templates into item_template table.", restoredCount);
    }
}


void ItemScalingRegistry::PreStageDungeonLoot()
{
    if (!sItemScalingConfig->PreStageDungeonLoot)
    {
        return;
    }

    LOG_INFO("server.loading", ">> ItemScaling: Pre-staging scalable dungeon and raid loot variants into database...");

    // Keep discovery in one SQL statement. AzerothCore's synchronous database pool may use a different
    // connection for each call, so connection-scoped TEMPORARY TABLE state is not safe across calls.
    std::string lootQuery = Acore::StringFormat(
        "SELECT DISTINCT it.entry, it.class, it.subclass, it.Quality, it.InventoryType, it.ItemLevel, "
        "       it.RequiredLevel, it.stat_type1, it.stat_value1, it.stat_type2, it.stat_value2, "
        "       it.stat_type3, it.stat_value3, it.stat_type4, it.stat_value4, it.stat_type5, "
        "       it.stat_value5, it.stat_type6, it.stat_value6, it.stat_type7, it.stat_value7, "
        "       it.stat_type8, it.stat_value8, it.stat_type9, it.stat_value9, it.stat_type10, "
        "       it.stat_value10, it.dmg_min1, it.dmg_max1, it.dmg_type1, it.dmg_min2, it.dmg_max2, "
        "       it.dmg_type2, it.armor, it.delay "
        "FROM item_template it "
        "JOIN ("
        "    SELECT CASE WHEN clt.Reference > 0 THEN r.Item ELSE clt.Item END AS item_id "
        "    FROM creature_loot_template clt "
        "    JOIN ("
        "        SELECT DISTINCT ct.lootid AS loot_id "
        "        FROM creature cr "
        "        JOIN instance_template inst ON inst.map = cr.map "
        "        JOIN creature_template base_ct ON base_ct.entry = cr.id "
        "        JOIN creature_template ct ON ct.entry IN ("
        "            base_ct.entry, base_ct.difficulty_entry_1, base_ct.difficulty_entry_2, "
        "            base_ct.difficulty_entry_3"
        "        ) "
        "        WHERE ct.lootid > 0"
        "    ) creature_loot ON creature_loot.loot_id = clt.Entry "
        "    LEFT JOIN reference_loot_template r ON clt.Reference > 0 AND r.Entry = clt.Reference "
        "    WHERE (clt.Reference = 0 AND clt.Item > 0) "
        "       OR (clt.Reference > 0 AND r.Item > 0) "
        "    UNION "
        "    SELECT CASE WHEN glt.Reference > 0 THEN r.Item ELSE glt.Item END AS item_id "
        "    FROM gameobject_loot_template glt "
        "    JOIN ("
        "        SELECT DISTINCT gt.Data1 AS loot_id "
        "        FROM gameobject go "
        "        JOIN instance_template inst ON inst.map = go.map "
        "        JOIN gameobject_template gt ON gt.entry = go.id "
        "        WHERE gt.type IN (3, 25) AND gt.Data1 > 0"
        "    ) gameobject_loot ON gameobject_loot.loot_id = glt.Entry "
        "    LEFT JOIN reference_loot_template r ON glt.Reference > 0 AND r.Entry = glt.Reference "
        "    WHERE (glt.Reference = 0 AND glt.Item > 0) "
        "       OR (glt.Reference > 0 AND r.Item > 0)"
        ") instance_loot ON instance_loot.item_id = it.entry "
        "WHERE it.class IN (2, 4) AND it.InventoryType NOT IN (0, 24, 27, 28) "
        "AND it.entry < {}",
        sItemScalingConfig->SyntheticEntryStart
    );

    QueryResult lootItemsRes = WorldDatabase.Query(lootQuery);
    if (!lootItemsRes)
    {
        LOG_INFO("server.loading", ">> ItemScaling: No instance loot items found to pre-stage.");
        return;
    }

    uint8 step = sItemScalingConfig->BracketStep;
    if (step < 1)
    {
        step = 2;
    }

    std::unordered_set<uint64> existingVariantKeys;
    QueryResult existingVariants = WorldDatabase.Query(
        "SELECT base_entry, target_effective_level, target_item_level, formula_version "
        "FROM scaled_item_variant WHERE formula_version = {}",
        sItemScalingConfig->FormulaVersion
    );

    if (existingVariants)
    {
        existingVariantKeys.reserve(existingVariants->GetRowCount());
        do
        {
            Field* fields = existingVariants->Fetch();
            existingVariantKeys.insert(PackVariantKey(
                fields[0].Get<uint32>(),
                fields[1].Get<uint8>(),
                fields[2].Get<uint16>(),
                fields[3].Get<uint8>()
            ));
        } while (existingVariants->NextRow());
    }

    WorldDatabaseTransaction trans = WorldDatabase.BeginTransaction();
    uint32 batchOps = 0;
    uint32 createdCount = 0;

    do
    {
        Field* bf = lootItemsRes->Fetch();
        ItemTemplate baseProto;
        baseProto.ItemId = bf[0].Get<uint32>();
        baseProto.Class = bf[1].Get<uint32>();
        baseProto.SubClass = bf[2].Get<uint32>();
        baseProto.Quality = bf[3].Get<uint32>();
        baseProto.InventoryType = bf[4].Get<uint32>();
        baseProto.ItemLevel = bf[5].Get<uint32>();
        baseProto.RequiredLevel = bf[6].Get<uint32>();

        if (!sItemScalingConfig->IsQualityEnabled(baseProto.Quality))
        {
            continue;
        }

        uint32 statCount = 0;
        for (uint32 s = 0; s < 10; ++s)
        {
            baseProto.ItemStat[s].ItemStatType = bf[7 + (s * 2)].Get<uint32>();
            baseProto.ItemStat[s].ItemStatValue = bf[8 + (s * 2)].Get<int32>();
            if (baseProto.ItemStat[s].ItemStatType != 0 || baseProto.ItemStat[s].ItemStatValue != 0)
            {
                ++statCount;
            }
        }
        baseProto.StatsCount = statCount;

        baseProto.Damage[0].DamageMin = bf[27].Get<float>();
        baseProto.Damage[0].DamageMax = bf[28].Get<float>();
        baseProto.Damage[0].DamageType = bf[29].Get<uint32>();
        baseProto.Damage[1].DamageMin = bf[30].Get<float>();
        baseProto.Damage[1].DamageMax = bf[31].Get<float>();
        baseProto.Damage[1].DamageType = bf[32].Get<uint32>();
        baseProto.Armor = bf[33].Get<uint32>();
        baseProto.Delay = bf[34].Get<uint32>();

        uint8 minLevel = std::max<uint8>(sItemScalingConfig->MinLevel, static_cast<uint8>(baseProto.RequiredLevel));
        if (minLevel == 0)
        {
            minLevel = 10;
        }

        uint8 origRefLevel = static_cast<uint8>(baseProto.RequiredLevel);
        if (origRefLevel == 0)
        {
            origRefLevel = static_cast<uint8>(std::clamp<uint32>(baseProto.ItemLevel, 1, 80));
        }

        if (sItemScalingConfig->IsLevelExcluded(origRefLevel))
        {
            continue;
        }

        for (uint8 targetLvl = minLevel; targetLvl <= sItemScalingConfig->MaxLevel; targetLvl += step)
        {
            uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(&baseProto, targetLvl, origRefLevel);
            if (targetIlvl == 0 || (targetIlvl == baseProto.ItemLevel && targetLvl == origRefLevel))
            {
                continue;
            }

            uint64 variantKey = PackVariantKey(
                baseProto.ItemId,
                targetLvl,
                targetIlvl,
                sItemScalingConfig->FormulaVersion
            );
            if (existingVariantKeys.find(variantKey) != existingVariantKeys.end())
                continue;

            uint32 newEntry = _nextSyntheticEntry.fetch_add(1);
            existingVariantKeys.insert(variantKey);

            ItemTemplate scaledProto = ItemScalingFormula::CreateScaledTemplate(
                &baseProto,
                newEntry,
                targetLvl,
                targetIlvl,
                sItemScalingConfig->FormulaVersion,
                targetLvl
            );

            trans->Append(BuildItemTemplateInsertSQL(scaledProto, baseProto.ItemId));
            trans->Append(BuildVariantInsertSQL(
                newEntry,
                baseProto.ItemId,
                targetLvl,
                targetIlvl,
                sItemScalingConfig->FormulaVersion
            ));
            batchOps += 2;
            ++createdCount;

            if (batchOps >= 500)
            {
                WorldDatabase.DirectCommitTransaction(trans);
                trans = WorldDatabase.BeginTransaction();
                batchOps = 0;
            }
        }
    } while (lootItemsRes->NextRow());

    if (batchOps > 0)
    {
        WorldDatabase.DirectCommitTransaction(trans);
    }

    LOG_INFO(
        "server.loading",
        ">> ItemScaling: Pre-staged {} synthetic item variants across instance loot tables.",
        createdCount
    );
}

void ItemScalingRegistry::OnLoadCustomDatabaseTable()
{
    if (_dbSynchronized)
    {
        return;
    }

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

    // 2. Resolve safe compact synthetic entry range
    ResolveSyntheticEntryRange();

    // 3. Ensure DBC stores required by ItemScalingFormula are loaded
    std::string dbcPath = sWorld->GetDataPath() + "dbc/";
    if (sRandomPropertiesPointsStore.GetNumRows() == 0)
    {
        sRandomPropertiesPointsStore.Load((dbcPath + "RandomPropertiesPoints.dbc").c_str());
    }
    if (sScalingStatValuesStore.GetNumRows() == 0)
    {
        sScalingStatValuesStore.Load((dbcPath + "ScalingStatValues.dbc").c_str());
    }

    // 4. Build baseline medians
    sItemScalingBaseline->BuildBaseline();

    // 5. Synchronize any existing scaled variants into item_template before LoadItemTemplates()
    SynchronizeExistingVariants();

    // 6. Pre-stage instance loot variants if enabled
    PreStageDungeonLoot();

    _dbSynchronized = true;
}

void ItemScalingRegistry::Initialize()
{
    if (_initialized)
    {
        return;
    }

    std::unique_lock lock(_cacheLock);
    _keyToEntry.clear();
    _baseToVariants.clear();

    if (sItemScalingConfig->SyntheticEntryStart == 0)
    {
        ResolveSyntheticEntryRange();
    }

    QueryResult result = WorldDatabase.Query(
        "SELECT variant_entry, base_entry, target_effective_level, target_item_level, formula_version "
        "FROM scaled_item_variant ORDER BY variant_entry ASC"
    );

    uint32 loadedCount = 0;
    uint32 maxVarEntry = sItemScalingConfig->SyntheticEntryStart;

    if (result)
    {
        _keyToEntry.reserve(result->GetRowCount() + 1000);

        do
        {
            Field* fields = result->Fetch();
            uint32 variantEntry = fields[0].Get<uint32>();
            uint32 baseEntry = fields[1].Get<uint32>();
            uint8 targetEffectiveLevel = fields[2].Get<uint8>();
            uint16 targetItemLevel = fields[3].Get<uint16>();
            uint8 formulaVersion = fields[4].Get<uint8>();

            if (variantEntry > maxVarEntry)
            {
                maxVarEntry = variantEntry;
            }

            uint64 key = PackVariantKey(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion);
            _keyToEntry[key] = variantEntry;

            VariantRecord rec;
            rec.variantEntry = variantEntry;
            rec.targetEffectiveLevel = targetEffectiveLevel;
            rec.targetItemLevel = targetItemLevel;
            _baseToVariants[baseEntry].push_back(rec);

            ++loadedCount;
        } while (result->NextRow());
    }

    _nextSyntheticEntry.store(std::max(maxVarEntry + 1, sItemScalingConfig->SyntheticEntryStart));

    // Ensure _itemTemplateStoreFast is pre-sized to accommodate new synthetic entries
    auto* fastStore = const_cast<std::vector<ItemTemplate*>*>(sObjectMgr->GetItemTemplateStoreFast());
    uint32 neededSize = std::max(_nextSyntheticEntry.load() + 20000, 100000u);
    if (fastStore && fastStore->size() < neededSize)
    {
        fastStore->resize(neededSize, nullptr);
    }

    _initialized = true;
    LOG_INFO("server.loading", ">> ItemScaling: Loaded and indexed {} scaled item variants into zero-stutter memory cache (next synthetic ID: {}).", loadedCount, _nextSyntheticEntry.load());
}

uint32 ItemScalingRegistry::GetOrCreateVariant(
    ItemTemplate const* baseProto,
    uint8 targetEffectiveLevel,
    uint16 targetItemLevel,
    uint8 formulaVersion,
    uint8 highestRealPlayerLevel)
{
    if (!baseProto)
    {
        return 0;
    }

    uint64 key = PackVariantKey(baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion);

    // 1. Fast-path: read lock O(1) lookup
    {
        std::shared_lock lock(_cacheLock);
        auto itr = _keyToEntry.find(key);
        if (itr != _keyToEntry.end())
        {
            return itr->second;
        }
    }

    // 2. Slow-path: write lock
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

    // Ensure _itemTemplateStoreFast vector has capacity in ObjectMgr
    auto* fastStore = const_cast<std::vector<ItemTemplate*>*>(sObjectMgr->GetItemTemplateStoreFast());
    if (fastStore && newEntry >= fastStore->size())
    {
        fastStore->resize(newEntry + 5000, nullptr);
    }

    // Construct scaled template deterministically
    ItemTemplate scaledProto = ItemScalingFormula::CreateScaledTemplate(
        baseProto,
        newEntry,
        targetEffectiveLevel,
        targetItemLevel,
        formulaVersion,
        highestRealPlayerLevel
    );

    // Store in our persistent deque so memory pointer never invalidates
    ItemTemplate& storedProto = _customTemplates.emplace_back(scaledProto);

    // Register into ObjectMgr fast store and container
    if (fastStore && newEntry < fastStore->size())
    {
        (*fastStore)[newEntry] = &storedProto;
    }
    auto* fullStore = const_cast<ItemTemplateContainer*>(sObjectMgr->GetItemTemplateStore());
    if (fullStore)
    {
        (*fullStore)[newEntry] = storedProto;
    }

    // Index in our memory maps
    _keyToEntry[key] = newEntry;

    VariantRecord rec;
    rec.variantEntry = newEntry;
    rec.targetEffectiveLevel = targetEffectiveLevel;
    rec.targetItemLevel = targetItemLevel;
    _baseToVariants[baseProto->ItemId].push_back(rec);

    // Persist to database asynchronously (zero map thread delay)
    WorldDatabase.Execute(BuildItemTemplateInsertSQL(storedProto, baseProto->ItemId));
    WorldDatabase.Execute(BuildVariantInsertSQL(newEntry, baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion));

    if (sItemScalingConfig->Debug)
    {
        LOG_INFO("module.ItemScaling", "ItemScalingRegistry: Created scaled variant {} for base item {} '{}' (Target Lvl: {}, Target Ilvl: {})",
            newEntry, baseProto->ItemId, baseProto->Name1, targetEffectiveLevel, targetItemLevel);
    }

    return newEntry;
}

uint32 ItemScalingRegistry::GetVariantEntry(uint32 baseEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion)
{
    uint64 key = PackVariantKey(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion);

    {
        std::shared_lock lock(_cacheLock);
        auto itr = _keyToEntry.find(key);
        if (itr != _keyToEntry.end())
        {
            return itr->second;
        }
    }

    // If not found in cache, create on demand
    ItemTemplate const* baseProto = sObjectMgr->GetItemTemplate(baseEntry);
    if (baseProto)
    {
        return GetOrCreateVariant(baseProto, targetEffectiveLevel, targetItemLevel, formulaVersion, targetEffectiveLevel);
    }

    // Fallback: closest match in memory
    std::shared_lock lock(_cacheLock);
    auto bItr = _baseToVariants.find(baseEntry);
    if (bItr != _baseToVariants.end() && !bItr->second.empty())
    {
        uint32 bestVariant = 0;
        int32 bestLevelDiff = 999;
        int32 bestIlvlDiff = 999;

        for (VariantRecord const& rec : bItr->second)
        {
            int32 levelDiff = std::abs(static_cast<int32>(rec.targetEffectiveLevel) - static_cast<int32>(targetEffectiveLevel));
            int32 ilvlDiff = std::abs(static_cast<int32>(rec.targetItemLevel) - static_cast<int32>(targetItemLevel));

            if (levelDiff < bestLevelDiff || (levelDiff == bestLevelDiff && ilvlDiff < bestIlvlDiff))
            {
                bestLevelDiff = levelDiff;
                bestIlvlDiff = ilvlDiff;
                bestVariant = rec.variantEntry;
            }
        }

        if (bestVariant != 0)
        {
            return bestVariant;
        }
    }

    return 0;
}
