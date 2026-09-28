#include "ItemScalingIdentity.h"
#include <cassert>

int main()
{
    ItemTemplate base{};
    base.Class = 4;
    base.SubClass = 6;
    base.SoundOverrideSubclass = -1;
    base.Material = -1;
    base.DisplayInfoID = 12345;
    base.InventoryType = 14;
    base.Sheath = 1;
    assert(ItemScalingIdentity::CanCapture(base));
    auto const identity = ItemScalingIdentity::Capture(base);
    ItemEntry dbc{100, 2, 7, -1, -1, 54321, 13, 3};
    assert(ItemScalingIdentity::Resolve(base, nullptr, true) == identity);
    assert(ItemScalingIdentity::Resolve(base, &dbc, false) == identity);
    auto const effective = ItemScalingIdentity::Resolve(base, &dbc, true);
    assert(effective.itemClass == 2 && effective.subClass == 7 && effective.inventoryType == 13);
    assert(effective.soundOverrideSubclass == -1 && effective.material == -1);
    assert(effective.displayId == 54321 && effective.sheath == 3);
    assert(effective != identity);
    ItemTemplate normalized = base;
    effective.Apply(normalized);
    assert(effective.Matches(normalized) && ItemScalingIdentity::CanCapture(normalized));
    // Every captured field participates, independently, in the drift check.
    auto changed = effective;
    ++changed.itemClass;
    assert(!changed.Matches(normalized));
    changed = effective;
    ++changed.subClass;
    assert(!changed.Matches(normalized));
    changed = effective;
    ++changed.soundOverrideSubclass;
    assert(!changed.Matches(normalized));
    changed = effective;
    ++changed.material;
    assert(!changed.Matches(normalized));
    changed = effective;
    ++changed.displayId;
    assert(!changed.Matches(normalized));
    changed = effective;
    ++changed.inventoryType;
    assert(!changed.Matches(normalized));
    changed = effective;
    ++changed.sheath;
    assert(!changed.Matches(normalized));
    normalized.Quality = 4;
    normalized.Armor = 987;
    assert(effective.Matches(normalized)); // Identity does not claim to snapshot rarity or stats.
    dbc.Material = 128;
    ItemScalingIdentity::Resolve(base, &dbc, true).Apply(normalized);
    assert(!ItemScalingIdentity::CanCapture(normalized));
    ItemTemplate scaled{};
    scaled.ItemId = 60000;
    scaled.ItemLevel = 150;
    scaled.RequiredLevel = 50;
    scaled.Armor = 123;
    scaled.Block = 45;
    scaled.ItemStat[0].ItemStatValue = 77;
    assert(!identity.Matches(scaled));
    identity.Apply(scaled);
    assert(identity.Matches(scaled));
    assert(scaled.ItemId == 60000 && scaled.ItemLevel == 150 && scaled.RequiredLevel == 50);
    assert(scaled.Armor == 123 && scaled.Block == 45 && scaled.ItemStat[0].ItemStatValue == 77);
    assert(scaled.SoundOverrideSubclass == -1 && scaled.Material == -1);
    assert(ItemScalingIdentity::CompatibleLootMetadata(base, scaled));
    scaled.Quality = 4;
    assert(!ItemScalingIdentity::CompatibleLootMetadata(base, scaled));
    scaled.Quality = base.Quality;
    scaled.ItemLimitCategory = 123;
    assert(!ItemScalingIdentity::CompatibleLootMetadata(base, scaled));
    base.ItemLimitCategory = 123;
    assert(ItemScalingIdentity::CompatibleLootMetadata(base, scaled));
    scaled.RandomProperty = 5;
    assert(!ItemScalingIdentity::CompatibleLootMetadata(base, scaled));
    base.Material = 128;
    assert(!ItemScalingIdentity::CanCapture(base));
    base.Material = -129;
    assert(!ItemScalingIdentity::CanCapture(base));
    base.Material = 0;
    base.InventoryType = 256;
    assert(!ItemScalingIdentity::CanCapture(base));
}
