/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_RUNTIME_TEMPLATE_H
#define _ITEM_SCALING_RUNTIME_TEMPLATE_H

#include "ItemScalingCommon.h"

namespace ItemScalingRuntimeTemplate
{
    // When a synthetic ID is absent from Item.dbc, core loading skips normal item validation.
    // Start from the validated base runtime template and restore only fields intentionally scaled by the module.
    inline ItemTemplate Build(ItemTemplate const& base, ItemTemplate const& persisted)
    {
        ItemTemplate runtime = base;

        runtime.ItemId = persisted.ItemId;
        runtime.ItemLevel = persisted.ItemLevel;
        runtime.RequiredLevel = persisted.RequiredLevel;
        runtime.StatsCount = persisted.StatsCount;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            runtime.ItemStat[i] = persisted.ItemStat[i];

        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
            runtime.Damage[i] = persisted.Damage[i];

        runtime.Armor = persisted.Armor;
        runtime.Block = persisted.Block;
        runtime.HolyRes = persisted.HolyRes;
        runtime.FireRes = persisted.FireRes;
        runtime.NatureRes = persisted.NatureRes;
        runtime.FrostRes = persisted.FrostRes;
        runtime.ShadowRes = persisted.ShadowRes;
        runtime.ArcaneRes = persisted.ArcaneRes;

        return runtime;
    }
}

#endif // _ITEM_SCALING_RUNTIME_TEMPLATE_H
