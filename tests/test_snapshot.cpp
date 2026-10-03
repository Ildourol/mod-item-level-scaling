#include "ItemScalingSnapshot.h"
#include <cassert>
#include <map>

int main()
{
    ItemTemplate item{};
    item.ItemId = 65000;
    item.Name1 = "Sword 'quoted', \\ path";
    item.Description = "Line one\nLine two";
    item.SoundOverrideSubclass = -1;
    item.Material = -1;
    item.AllowableClass = -1;
    item.AllowableRace = -1;
    item.ItemLevel = 150;
    item.RequiredLevel = 50;
    item.StatsCount = 2;
    item.ItemStat[0] = {ITEM_MOD_STAMINA, 75};
    item.ItemStat[1] = {ITEM_MOD_INTELLECT, -3};
    item.ItemStat[2] = {ITEM_MOD_STRENGTH, 999}; // Unused storage must never become a real bonus.
    item.Damage[0].DamageMin = 25.125f;
    item.Damage[0].DamageMax = 50.25f;
    item.Armor = 1234;
    item.Block = 56;
    item.HolyRes = -12;
    item.ArcaneRes = 34;
    item.Spells[4].SpellId = 123;
    item.Spells[4].SpellCharges = -2;
    item.Spells[4].SpellCooldown = -1;
    item.Socket[2].Color = 4;
    item.Socket[2].Content = 567;
    item.Duration = 12345;
    item.ItemLimitCategory = 89;
    item.DisenchantID = 100;
    item.RandomProperty = 0;
    item.RandomSuffix = 0;

    std::string sql = ItemScalingSnapshot::Insert(item, "staged");
    auto columnsStart = sql.find('(') + 1;
    auto columnsEnd = sql.find(") VALUES (");
    auto valuesStart = columnsEnd + std::string(") VALUES (").size();
    std::map<std::string, std::string> fields;
    std::istringstream columns(sql.substr(columnsStart, columnsEnd - columnsStart));
    std::istringstream values(sql.substr(valuesStart, sql.size() - valuesStart - 1));
    std::string column, value;
    while (std::getline(columns, column, ','))
    {
        assert(std::getline(values, value, ','));
        assert(fields.emplace(column.substr(1, column.size() - 2), value).second);
    }
    assert(!std::getline(values, value, ','));
    assert(fields.at("entry") == "65000");
    assert(fields.at("SoundOverrideSubclass") == "-1" && fields.at("Material") == "-1");
    assert(fields.at("AllowableClass") == "-1" && fields.at("AllowableRace") == "-1");
    assert(fields.at("ItemLevel") == "150" && fields.at("RequiredLevel") == "50");
    assert(fields.at("stat_value1") == "75" && fields.at("stat_value2") == "-3");
    assert(fields.at("stat_type3") == "0" && fields.at("stat_value3") == "0");
    assert(fields.at("stat_value10") == "0");
    assert(fields.at("dmg_min1") == "25.125" && fields.at("dmg_max1") == "50.25");
    assert(fields.at("armor") == "1234" && fields.at("block") == "56");
    assert(fields.at("holy_res") == "-12" && fields.at("arcane_res") == "34");
    assert(fields.at("spellid_5") == "123" && fields.at("spellcharges_5") == "-2");
    assert(fields.at("spellcooldown_5") == "-1");
    assert(fields.at("socketColor_3") == "4" && fields.at("socketContent_3") == "567");
    assert(fields.at("duration") == "12345" && fields.at("ItemLimitCategory") == "89");
    assert(fields.at("DisenchantID") == "100" && fields.at("ScriptName") == "''");
    assert(fields.at("RandomProperty") == "0" && fields.at("RandomSuffix") == "0");
    assert(fields.at("name") == "CONVERT(0x53776f7264202771756f746564272c205c2070617468 USING utf8mb4)");
    assert(fields.at("description") == "CONVERT(0x4c696e65206f6e650a4c696e652074776f USING utf8mb4)");
    assert(ItemScalingSnapshot::Text("") == "''");
    assert(ItemScalingSnapshot::Text("\xC3\xA6") == "CONVERT(0xc3a6 USING utf8mb4)");
}
