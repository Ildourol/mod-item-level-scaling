#include "ItemScalingTarget.h"
#include <cassert>

namespace
{
    ItemScalingTarget::Input BaseInput()
    {
        ItemScalingTarget::Input input;
        input.playerLevel = 60;
        input.creatureMinLevel = 55;
        input.creatureSourceLevel = 60;
        input.instanceMaxLevel = 63;
        input.observedCreatureLevel = 60;
        input.floor = 5;
        input.ceiling = 3;
        input.minLevel = 1;
        input.maxLevel = 80;
        input.dynamic = true;
        input.hasCreature = true;
        input.realPlayersOnly = true;
        return input;
    }
}

int main()
{
    using ItemScalingTarget::IsExternallyScaledCreature;
    using ItemScalingTarget::Resolve;

    auto input = BaseInput();
    assert(!IsExternallyScaledCreature(input));
    assert(Resolve(input) == 60);

    input = BaseInput();
    input.creatureSourceLevel = 55;
    input.observedCreatureLevel = 55;
    assert(Resolve(input) == 55);

    input = BaseInput();
    input.creatureMinLevel = 63;
    input.creatureSourceLevel = 63;
    input.observedCreatureLevel = 63;
    assert(Resolve(input) == 63);

    input = BaseInput();
    input.dynamic = false;
    input.observedCreatureLevel = 70;
    input.realPlayersOnly = false;
    assert(Resolve(input) == 60);

    input = BaseInput();
    input.hasCreature = false;
    assert(Resolve(input) == 60);

    input = BaseInput();
    input.observedCreatureLevel = 70;
    input.realPlayersOnly = false;
    assert(IsExternallyScaledCreature(input));
    assert(Resolve(input) == 70);

    input = BaseInput();
    input.observedCreatureLevel = 70;
    input.realPlayersOnly = true;
    assert(IsExternallyScaledCreature(input));
    assert(Resolve(input) == 60);

    input = BaseInput();
    input.observedCreatureLevel = 70;
    input.realPlayersOnly = false;
    input.maxLevel = 65;
    assert(Resolve(input) == 65);

    input = BaseInput();
    input.playerLevel = 2;
    input.creatureMinLevel = 1;
    input.creatureSourceLevel = 1;
    input.instanceMaxLevel = 5;
    input.observedCreatureLevel = 1;
    assert(Resolve(input) == 1);

    input = BaseInput();
    input.playerLevel = 80;
    input.creatureMinLevel = 83;
    input.creatureSourceLevel = 83;
    input.instanceMaxLevel = 83;
    input.observedCreatureLevel = 83;
    assert(Resolve(input) == 80);

    input = BaseInput();
    input.minLevel = 70;
    input.maxLevel = 20;
    input.playerLevel = 60;
    input.dynamic = false;
    assert(Resolve(input) == 60);
}
