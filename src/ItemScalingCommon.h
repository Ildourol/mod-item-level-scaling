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
#include <cstddef>
#include <functional>
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
    uint8  requiredLevel{0};

    bool operator<(VariantKey const& o) const
    {
        return std::tie(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion, requiredLevel) <
               std::tie(o.baseEntry, o.targetEffectiveLevel, o.targetItemLevel, o.formulaVersion, o.requiredLevel);
    }

    bool operator==(VariantKey const& o) const
    {
        return std::tie(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion, requiredLevel) ==
               std::tie(o.baseEntry, o.targetEffectiveLevel, o.targetItemLevel, o.formulaVersion, o.requiredLevel);
    }
};

struct VariantKeyHash
{
    [[nodiscard]] std::size_t operator()(VariantKey const& key) const noexcept
    {
        std::size_t seed = 0;
        auto combine = [&seed](auto value)
        {
            using Value = decltype(value);
            seed ^= std::hash<Value>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

        combine(key.baseEntry);
        combine(key.targetEffectiveLevel);
        combine(key.targetItemLevel);
        combine(key.formulaVersion);
        combine(key.requiredLevel);
        return seed;
    }
};

#endif // _ITEM_SCALING_COMMON_H
