#include "ItemScalingRuntimeTemplate.h"
#include <cassert>

int main()
{
    ItemTemplate base{};
    base.ItemId = 12345;
    base.Class = ITEM_CLASS_WEAPON;
    base.SubClass = ITEM_SUBCLASS_WEAPON_SWORD;
    base.SoundOverrideSubclass = -1;
    base.Name1 = "Validated base";
    base.DisplayInfoID = 4567;
    base.InventoryType = INVTYPE_WEAPON;
    base.Material = 1;
    base.Sheath = 3;
    base.BuyPrice = 100;
    base.SellPrice = 25;
    base.RandomProperty = 42;

    ItemTemplate persisted = base;
    persisted.ItemId = 60000;
    persisted.Class = ITEM_CLASS_MISC;
    persisted.SubClass = 0;
    persisted.SoundOverrideSubclass = 0;
    persisted.Name1 = "Raw persisted row";
    persisted.DisplayInfoID = 0;
    persisted.InventoryType = INVTYPE_NON_EQUIP;
    persisted.Material = 0;
    persisted.Sheath = 0;
    persisted.BuyPrice = 999;
    persisted.SellPrice = 999;
    persisted.RandomProperty = 999;
    persisted.ItemLevel = 80;
    persisted.RequiredLevel = 75;
    persisted.StatsCount = 2;
    persisted.ItemStat[0].ItemStatType = ITEM_MOD_STRENGTH;
    persisted.ItemStat[0].ItemStatValue = 35;
    persisted.ItemStat[1].ItemStatType = ITEM_MOD_STAMINA;
    persisted.ItemStat[1].ItemStatValue = 52;
    persisted.Damage[0].DamageMin = 101.0f;
    persisted.Damage[0].DamageMax = 151.0f;
    persisted.Damage[0].DamageType = SPELL_SCHOOL_NORMAL;
    persisted.Armor = 77;
    persisted.Block = 9;
    persisted.HolyRes = 7;
    persisted.FireRes = 11;
    persisted.NatureRes = 12;
    persisted.FrostRes = 13;
    persisted.ShadowRes = 17;
    persisted.ArcaneRes = 19;

    ItemTemplate runtime = ItemScalingRuntimeTemplate::Build(base, persisted);

    assert(runtime.ItemId == persisted.ItemId);
    assert(runtime.ItemLevel == persisted.ItemLevel);
    assert(runtime.RequiredLevel == persisted.RequiredLevel);
    assert(runtime.StatsCount == persisted.StatsCount);
    assert(runtime.ItemStat[0].ItemStatValue == persisted.ItemStat[0].ItemStatValue);
    assert(runtime.ItemStat[1].ItemStatValue == persisted.ItemStat[1].ItemStatValue);
    assert(runtime.Damage[0].DamageMin == persisted.Damage[0].DamageMin);
    assert(runtime.Damage[0].DamageMax == persisted.Damage[0].DamageMax);
    assert(runtime.Armor == persisted.Armor);
    assert(runtime.Block == persisted.Block);
    assert(runtime.HolyRes == persisted.HolyRes);
    assert(runtime.FireRes == persisted.FireRes);
    assert(runtime.NatureRes == persisted.NatureRes);
    assert(runtime.FrostRes == persisted.FrostRes);
    assert(runtime.ShadowRes == persisted.ShadowRes);
    assert(runtime.ArcaneRes == persisted.ArcaneRes);

    assert(runtime.Class == base.Class);
    assert(runtime.SubClass == base.SubClass);
    assert(runtime.SoundOverrideSubclass == base.SoundOverrideSubclass);
    assert(runtime.Name1 == base.Name1);
    assert(runtime.DisplayInfoID == base.DisplayInfoID);
    assert(runtime.InventoryType == base.InventoryType);
    assert(runtime.Material == base.Material);
    assert(runtime.Sheath == base.Sheath);
    assert(runtime.BuyPrice == base.BuyPrice);
    assert(runtime.SellPrice == base.SellPrice);
    assert(runtime.RandomProperty == base.RandomProperty);
}
