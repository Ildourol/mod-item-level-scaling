/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef MOD_ITEM_SCALING_SNAPSHOT_H
#define MOD_ITEM_SCALING_SNAPSHOT_H

#include "ItemTemplate.h"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace ItemScalingSnapshot
{
    // Hex literals do not depend on SQL escaping, connection checkout, SQL mode or locale.
    inline std::string Text(std::string const& value)
    {
        if (value.empty())
            return "''";
        static char const digits[] = "0123456789abcdef";
        std::string result = "CONVERT(0x";
        for (unsigned char c : value)
        {
            result += digits[c >> 4];
            result += digits[c & 15];
        }
        return result + " USING utf8mb4)";
    }

    // Serialize the complete, normalized RAM template. No INSERT ... SELECT from a mutable
    // base row: the snapshot must load identically even if the base changes before restart.
    inline std::string Insert(ItemTemplate const& item, std::string const& table)
    {
        std::ostringstream columns;
        std::ostringstream values;
        values.imbue(std::locale::classic());
        values << std::setprecision(std::numeric_limits<float>::max_digits10);
        bool first = true;
        auto field = [&](std::string const& name, auto value)
        {
            if (!first)
            {
                columns << ',';
                values << ',';
            }
            first = false;
            columns << '`' << name << '`';
            values << value;
        };

        field("entry", item.ItemId);
        field("class", item.Class);
        field("subclass", item.SubClass);
        field("SoundOverrideSubclass", item.SoundOverrideSubclass);
        field("name", Text(item.Name1));
        field("displayid", item.DisplayInfoID);
        field("Quality", item.Quality);
        field("Flags", static_cast<uint32>(item.Flags));
        field("FlagsExtra", static_cast<uint32>(item.Flags2));
        field("BuyCount", item.BuyCount);
        field("BuyPrice", item.BuyPrice);
        field("SellPrice", item.SellPrice);
        field("InventoryType", item.InventoryType);
        field("AllowableClass", item.AllowableClass);
        field("AllowableRace", item.AllowableRace);
        field("ItemLevel", item.ItemLevel);
        field("RequiredLevel", item.RequiredLevel);
        field("RequiredSkill", item.RequiredSkill);
        field("RequiredSkillRank", item.RequiredSkillRank);
        field("requiredspell", item.RequiredSpell);
        field("requiredhonorrank", item.RequiredHonorRank);
        field("RequiredCityRank", item.RequiredCityRank);
        field("RequiredReputationFaction", item.RequiredReputationFaction);
        field("RequiredReputationRank", item.RequiredReputationRank);
        field("maxcount", item.MaxCount);
        field("stackable", item.Stackable);
        field("ContainerSlots", item.ContainerSlots);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            bool populated = i < item.StatsCount;
            field("stat_type" + std::to_string(i + 1), populated ? item.ItemStat[i].ItemStatType : 0);
            field("stat_value" + std::to_string(i + 1), populated ? item.ItemStat[i].ItemStatValue : 0);
        }
        field("ScalingStatDistribution", item.ScalingStatDistribution);
        field("ScalingStatValue", item.ScalingStatValue);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            field("dmg_min" + std::to_string(i + 1), item.Damage[i].DamageMin);
            field("dmg_max" + std::to_string(i + 1), item.Damage[i].DamageMax);
            field("dmg_type" + std::to_string(i + 1), item.Damage[i].DamageType);
        }
        field("armor", item.Armor);
        field("holy_res", item.HolyRes);
        field("fire_res", item.FireRes);
        field("nature_res", item.NatureRes);
        field("frost_res", item.FrostRes);
        field("shadow_res", item.ShadowRes);
        field("arcane_res", item.ArcaneRes);
        field("delay", item.Delay);
        field("ammo_type", item.AmmoType);
        field("RangedModRange", item.RangedModRange);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            std::string suffix = "_" + std::to_string(i + 1);
            field("spellid" + suffix, item.Spells[i].SpellId);
            field("spelltrigger" + suffix, item.Spells[i].SpellTrigger);
            field("spellcharges" + suffix, item.Spells[i].SpellCharges);
            field("spellppmRate" + suffix, item.Spells[i].SpellPPMRate);
            field("spellcooldown" + suffix, item.Spells[i].SpellCooldown);
            field("spellcategory" + suffix, item.Spells[i].SpellCategory);
            field("spellcategorycooldown" + suffix, item.Spells[i].SpellCategoryCooldown);
        }
        field("bonding", item.Bonding);
        field("description", Text(item.Description));
        field("PageText", item.PageText);
        field("LanguageID", item.LanguageID);
        field("PageMaterial", item.PageMaterial);
        field("startquest", item.StartQuest);
        field("lockid", item.LockID);
        field("Material", item.Material);
        field("sheath", item.Sheath);
        field("RandomProperty", item.RandomProperty);
        field("RandomSuffix", item.RandomSuffix);
        field("block", item.Block);
        field("itemset", item.ItemSet);
        field("MaxDurability", item.MaxDurability);
        field("area", item.Area);
        field("Map", item.Map);
        field("BagFamily", item.BagFamily);
        field("TotemCategory", item.TotemCategory);
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        {
            field("socketColor_" + std::to_string(i + 1), item.Socket[i].Color);
            field("socketContent_" + std::to_string(i + 1), item.Socket[i].Content);
        }
        field("socketBonus", item.socketBonus);
        field("GemProperties", item.GemProperties);
        field("RequiredDisenchantSkill", item.RequiredDisenchantSkill);
        field("ArmorDamageModifier", item.ArmorDamageModifier);
        field("duration", item.Duration);
        field("ItemLimitCategory", item.ItemLimitCategory);
        field("HolidayId", item.HolidayId);
        // Scalable equipment is already required to have ScriptId == 0.
        field("ScriptName", "''");
        field("DisenchantID", item.DisenchantID);
        field("FoodType", item.FoodType);
        field("minMoneyLoot", item.MinMoneyLoot);
        field("maxMoneyLoot", item.MaxMoneyLoot);
        field("flagsCustom", static_cast<uint32>(item.FlagsCu));
        return "INSERT INTO " + table + " (" + columns.str() + ") VALUES (" + values.str() + ")";
    }
}

#endif
