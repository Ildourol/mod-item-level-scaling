/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_WORLD_SCRIPT_H
#define _ITEM_SCALING_WORLD_SCRIPT_H

#include "WorldScript.h"

class ItemScalingWorldScript : public WorldScript
{
public:
    ItemScalingWorldScript();

    void OnStartup() override;
    void OnAfterConfigLoad(bool reload) override;
};

void AddItemScalingWorldScripts();

#endif // _ITEM_SCALING_WORLD_SCRIPT_H
