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

// Increment only when generator implementation changes can alter persisted template values.
// Revision 1 represents every variant generated before this field was introduced.
inline constexpr uint8 ITEM_SCALING_GENERATOR_REVISION = 1;

struct VariantKey
{
    uint32 baseEntry{0};
    uint8  targetEffectiveLevel{0};
    uint16 targetItemLevel{0};
    uint8  formulaVersion{1};
    uint8  generatorRevision{ITEM_SCALING_GENERATOR_REVISION};
    uint8  requiredLevel{0};

    bool operator<(VariantKey const& o) const
    {
        return std::tie(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion, generatorRevision,
                   requiredLevel) <
               std::tie(o.baseEntry, o.targetEffectiveLevel, o.targetItemLevel, o.formulaVersion,
                   o.generatorRevision, o.requiredLevel);
    }

    bool operator==(VariantKey const& o) const
    {
        return std::tie(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion, generatorRevision,
                   requiredLevel) ==
               std::tie(o.baseEntry, o.targetEffectiveLevel, o.targetItemLevel, o.formulaVersion,
                   o.generatorRevision, o.requiredLevel);
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
        combine(key.generatorRevision);
        combine(key.requiredLevel);
        return seed;
    }
};

#endif // _ITEM_SCALING_COMMON_H
