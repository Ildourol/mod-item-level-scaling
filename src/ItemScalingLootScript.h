/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_LOOT_SCRIPT_H
#define _ITEM_SCALING_LOOT_SCRIPT_H

#include "MiscScript.h"

class Map;
struct CreatureTemplate;

class ItemScalingLootScript : public MiscScript
{
public:
    ItemScalingLootScript();

    static Player* GetEligibleOwner(Map const* map);
    static void PrepareLoot(Loot* loot, LootStore const& store, Player* owner,
        CreatureTemplate const* sourceOverride = nullptr, bool prewarm = false);

    void OnAfterLootTemplateProcess(Loot* loot, LootTemplate const* tab, LootStore const& store, Player* lootOwner, bool personal, bool noEmptyError, uint16 lootMode) override;
};

void AddItemScalingLootScripts();

#endif // _ITEM_SCALING_LOOT_SCRIPT_H
