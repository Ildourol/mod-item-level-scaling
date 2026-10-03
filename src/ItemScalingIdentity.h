/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_IDENTITY_H
#define _ITEM_SCALING_IDENTITY_H

#include "ItemScalingCommon.h"

// Seven DBC-enforced identity fields. SQL widths match item_template, including signed tinyints.
struct ItemScalingIdentity
{
    uint32 itemClass;
    uint32 subClass;
    int32 soundOverrideSubclass;
    int32 material;
    uint32 displayId;
    uint32 inventoryType;
    uint32 sheath;

    static bool CanCapture(ItemTemplate const& item)
    {
        return item.Class <= 255 && item.SubClass <= 255 && item.InventoryType <= 255 && item.Sheath <= 255 &&
            item.SoundOverrideSubclass >= -128 && item.SoundOverrideSubclass <= 127 &&
            item.Material >= -128 && item.Material <= 127;
    }

    static ItemScalingIdentity Capture(ItemTemplate const& item)
    {
        return {item.Class, item.SubClass, item.SoundOverrideSubclass, item.Material, item.DisplayInfoID,
            item.InventoryType, item.Sheath};
    }

    bool Matches(ItemTemplate const& item) const
    {
        return item.Class == itemClass && item.SubClass == subClass &&
            item.SoundOverrideSubclass == soundOverrideSubclass && item.Material == material &&
            item.DisplayInfoID == displayId && item.InventoryType == inventoryType && item.Sheath == sheath;
    }

    // Synthetic IDs skip core DBC validation. Withhold rows whose inherited loot semantics differ.
    static bool CompatibleLootMetadata(ItemTemplate const& base, ItemTemplate const& variant, bool baked = false)
    {
        if (base.Quality != variant.Quality || base.Flags != variant.Flags || base.Flags2 != variant.Flags2 || base.FlagsCu != variant.FlagsCu ||
            base.MaxCount != variant.MaxCount || base.ItemLimitCategory != variant.ItemLimitCategory ||
            base.StartQuest != variant.StartQuest || base.RequiredSkill != variant.RequiredSkill ||
            base.AllowableClass != variant.AllowableClass || base.AllowableRace != variant.AllowableRace ||
            base.Bonding != variant.Bonding || base.Stackable != variant.Stackable)
            return false;
        if (baked ? (variant.RandomProperty != 0 || variant.RandomSuffix != 0) :
            (base.RandomProperty != variant.RandomProperty || base.RandomSuffix != variant.RandomSuffix))
            return false;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            if (base.Spells[i].SpellId != variant.Spells[i].SpellId)
                return false;
        return true;
    }

    // Used only on module-owned temporary templates before their SQL insert.
    void Apply(ItemTemplate& item) const
    {
        item.Class = itemClass;
        item.SubClass = subClass;
        item.SoundOverrideSubclass = soundOverrideSubclass;
        item.Material = material;
        item.DisplayInfoID = displayId;
        item.InventoryType = inventoryType;
        item.Sheath = sheath;
    }
};

#endif // _ITEM_SCALING_IDENTITY_H
