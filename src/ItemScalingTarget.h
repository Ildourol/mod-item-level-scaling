/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef ITEM_SCALING_TARGET_H
#define ITEM_SCALING_TARGET_H

#include <algorithm>
#include <cstdint>

namespace ItemScalingTarget
{
    struct Input
    {
        std::uint8_t playerLevel{1};
        std::uint8_t creatureMinLevel{1};
        std::uint8_t creatureSourceLevel{1};
        std::uint8_t instanceMaxLevel{1};
        std::uint8_t observedCreatureLevel{1};
        std::uint8_t floor{3};
        std::uint8_t ceiling{0};
        std::uint8_t minLevel{1};
        std::uint8_t maxLevel{80};
        bool dynamic{true};
        bool hasCreature{false};
        bool realPlayersOnly{true};
    };

    [[nodiscard]] inline bool IsExternallyScaledCreature(Input const& input)
    {
        return input.hasCreature &&
            (input.observedCreatureLevel < input.creatureMinLevel ||
             input.observedCreatureLevel > input.creatureSourceLevel);
    }

    [[nodiscard]] inline std::uint8_t Resolve(Input const& input)
    {
        std::uint8_t minimum = std::clamp<std::uint8_t>(std::min(input.minLevel, input.maxLevel), 1, 80);
        std::uint8_t maximum = std::clamp<std::uint8_t>(std::max(input.minLevel, input.maxLevel), 1, 80);
        std::uint8_t playerLevel = std::clamp<std::uint8_t>(input.playerLevel, 1, 80);
        std::uint8_t target = playerLevel;

        if (input.dynamic && input.hasCreature)
        {
            std::int32_t mobLevel = static_cast<std::int32_t>(input.observedCreatureLevel);
            std::int32_t raw = mobLevel - static_cast<std::int32_t>(input.floor);

            std::int32_t ceilingLevel = std::min<std::int32_t>(
                static_cast<std::int32_t>(playerLevel) + static_cast<std::int32_t>(input.ceiling),
                static_cast<std::int32_t>(maximum));
            std::int32_t floorLevel = static_cast<std::int32_t>(minimum);

            target = static_cast<std::uint8_t>(std::clamp<std::int32_t>(raw, floorLevel, ceilingLevel));
        }

        return std::clamp<std::uint8_t>(target, minimum, maximum);
    }

    [[nodiscard]] inline std::uint8_t GetNativeReferenceLevel(std::uint8_t requiredLevel, std::uint32_t itemLevel)
    {
        if (requiredLevel > 0)
            return requiredLevel;
        return static_cast<std::uint8_t>(std::clamp<std::uint32_t>(itemLevel, 1, 80));
    }

    [[nodiscard]] inline bool IsNativeTargetMatch(
        std::uint8_t nativeRefLevel,
        std::uint8_t requestedTarget,
        std::uint8_t bracketedTarget,
        std::uint8_t playerLevel = 0)
    {
        return (nativeRefLevel == requestedTarget ||
                nativeRefLevel == bracketedTarget ||
                (playerLevel > 0 && nativeRefLevel == playerLevel));
    }
}

#endif

