/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_LOOT_SCRIPT_H
#define _ITEM_SCALING_LOOT_SCRIPT_H

#include "MiscScript.h"

class ItemScalingLootScript : public MiscScript
{
public:
    ItemScalingLootScript();

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* tab, LootStore const& store, Player* lootOwner, bool personal, bool noEmptyError, uint16 lootMode) override;
};

void AddItemScalingLootScripts();

#endif // _ITEM_SCALING_LOOT_SCRIPT_H
