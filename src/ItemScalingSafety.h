#ifndef ITEM_SCALING_SAFETY_H
#define ITEM_SCALING_SAFETY_H

#include <algorithm>
#include <cstdint>
#include <utility>

namespace ItemScalingSafety
{
    inline std::pair<std::uint8_t, std::uint8_t> LevelRange(std::uint32_t low, std::uint32_t high)
    {
        low = std::clamp<std::uint32_t>(low, 1, 80);
        high = std::clamp<std::uint32_t>(high, 1, 80);
        if (low > high)
            std::swap(low, high);
        return {static_cast<std::uint8_t>(low), static_cast<std::uint8_t>(high)};
    }

    inline std::uint8_t Bracket(std::uint8_t level, std::uint8_t low, std::uint8_t high, std::uint8_t step)
    {
        level = std::clamp(level, low, high);
        step = std::max<std::uint8_t>(1, step);
        if (level == high)
            return high;
        return static_cast<std::uint8_t>(low + ((level - low) / step) * step);
    }

    inline std::uint64_t AllocationStart(std::uint64_t highestItem, std::uint64_t highestVariant,
        bool automatic, std::uint32_t configuredStart, std::uint32_t configuredOffset)
    {
        // Database IDs are uint32. Arithmetic stays uint64 until the caller checks its configured cap.
        std::uint64_t highest = std::max(highestItem, highestVariant);
        std::uint64_t offset = highestVariant ? 1 : std::max<std::uint32_t>(1, configuredOffset);
        std::uint64_t start = automatic ? std::max<std::uint64_t>(60000, highest + offset) : configuredStart;
        return std::max(start, highest + 1);
    }
}

#endif
