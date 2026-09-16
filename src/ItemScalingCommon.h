/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_COMMON_H
#define _ITEM_SCALING_COMMON_H

#include "Common.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "ItemTemplate.h"
#include "SharedDefines.h"
#include <tuple>

enum ItemScalingMethod : uint8
{
    SCALING_METHOD_DYNAMIC = 0,
    SCALING_METHOD_FIXED   = 1
};

enum RequiredLevelPolicy : uint8
{
    REQ_POLICY_TARGET_CAPPED_PLAYER = 0,
    REQ_POLICY_PLAYER               = 1,
    REQ_POLICY_TARGET               = 2
};

struct VariantKey
{
    uint32 baseEntry{0};
    uint8  targetEffectiveLevel{0};
    uint16 targetItemLevel{0};
    uint8  formulaVersion{1};

    bool operator<(VariantKey const& o) const
    {
        return std::tie(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion) <
               std::tie(o.baseEntry, o.targetEffectiveLevel, o.targetItemLevel, o.formulaVersion);
    }

    bool operator==(VariantKey const& o) const
    {
        return std::tie(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion) ==
               std::tie(o.baseEntry, o.targetEffectiveLevel, o.targetItemLevel, o.formulaVersion);
    }
};

#endif // _ITEM_SCALING_COMMON_H
