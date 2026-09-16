/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

void AddItemScalingWorldScripts();
void AddItemScalingLootScripts();

void AddItemScalingScripts()
{
    AddItemScalingWorldScripts();
    AddItemScalingLootScripts();
}

void Addmod_item_level_scalingScripts()
{
    AddItemScalingScripts();
}

void Addmod_instance_item_scalingScripts()
{
    AddItemScalingScripts();
}

void Addmod_scaled_instance_lootScripts()
{
    AddItemScalingScripts();
}
