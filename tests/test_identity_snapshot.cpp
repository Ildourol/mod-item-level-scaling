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
    // Restart validation accepts fixed baked bonuses only for a baked key.
    base.RandomProperty = 5;
    scaled.RandomProperty = 0;
    assert(!ItemScalingIdentity::CompatibleLootMetadata(base, scaled));
    assert(ItemScalingIdentity::CompatibleLootMetadata(base, scaled, true));
    scaled.RandomSuffix = 7;
    assert(!ItemScalingIdentity::CompatibleLootMetadata(base, scaled, true));
    scaled.RandomSuffix = 0;
    base.RandomProperty = 0;
    base.RandomSuffix = 7;
    assert(ItemScalingIdentity::CompatibleLootMetadata(base, scaled, true));
    scaled.Bonding = base.Bonding + 1;
    assert(!ItemScalingIdentity::CompatibleLootMetadata(base, scaled, true));
    base.Material = 128;
    assert(!ItemScalingIdentity::CanCapture(base));
    base.Material = -129;
    assert(!ItemScalingIdentity::CanCapture(base));
    base.Material = 0;
    base.InventoryType = 256;
    assert(!ItemScalingIdentity::CanCapture(base));
}
