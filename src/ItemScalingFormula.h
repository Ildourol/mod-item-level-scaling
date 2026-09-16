/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_FORMULA_H
#define _ITEM_SCALING_FORMULA_H

#include "ItemScalingCommon.h"

namespace ItemScalingFormula
{
    [[nodiscard]] int8 GetSlotFamily(uint32 inventoryType);

    [[nodiscard]] double GetRandomPropertiesPoints(uint32 itemLevel, uint32 quality, uint8 slotFamily);

    [[nodiscard]] double GetBudgetRatio(uint32 baseIlvl, uint32 targetIlvl, uint32 quality, uint8 slotFamily);

    [[nodiscard]] int32 ScaleAdditiveStat(int32 v0, double r, bool preserveNonZero);

    [[nodiscard]] uint32 ScaleArmor(ItemTemplate const* baseProto, double rBudget, uint8 l0, uint8 lTarget);

    void ScaleWeaponDamage(ItemTemplate const* baseProto, ItemTemplate& scaledProto, double rBudget, uint8 l0, uint8 lTarget);

    [[nodiscard]] uint8 CalculateRequiredLevel(ItemTemplate const* baseProto, uint8 targetEffectiveLevel, uint8 highestRealPlayerLevel);

    [[nodiscard]] bool IsScalableEquipment(ItemTemplate const* proto);

    [[nodiscard]] ItemTemplate CreateScaledTemplate(ItemTemplate const* baseProto, uint32 newEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel);
}

#endif // _ITEM_SCALING_FORMULA_H
