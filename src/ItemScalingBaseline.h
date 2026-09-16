/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_BASELINE_H
#define _ITEM_SCALING_BASELINE_H

#include "ItemScalingCommon.h"
#include <array>
#include <vector>

constexpr uint8 MAX_BASELINE_LEVEL = 83;
constexpr uint8 MAX_BASELINE_QUALITY = 8;
constexpr uint8 MAX_BASELINE_FAMILY = 5;

class ItemScalingBaseline
{
public:
    static ItemScalingBaseline* instance();

    void BuildBaseline();

    [[nodiscard]] double GetMedianItemLevel(uint8 level, uint32 quality, uint8 slotFamily) const;
    [[nodiscard]] uint16 CalculateTargetItemLevel(ItemTemplate const* baseProto, uint8 targetLevel, uint8 originalRefLevel) const;

private:
    // _medians[level][quality][family]
    std::array<std::array<std::array<double, MAX_BASELINE_FAMILY>, MAX_BASELINE_QUALITY>, MAX_BASELINE_LEVEL + 1> _medians{};
    bool _initialized{false};

    void InterpolateMissingEntries();
};

#define sItemScalingBaseline ItemScalingBaseline::instance()

#endif // _ITEM_SCALING_BASELINE_H
