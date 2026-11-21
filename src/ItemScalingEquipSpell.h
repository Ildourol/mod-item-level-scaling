/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_EQUIP_SPELL_H
#define _ITEM_SCALING_EQUIP_SPELL_H

#include "ItemScalingCommon.h"

namespace ItemScalingEquipSpell
{
    void Initialize();
    void Scale(ItemTemplate& item, double budgetRatio);
    bool IsEquivalent(int32 originalId, int32 replacementId);
}

#endif
