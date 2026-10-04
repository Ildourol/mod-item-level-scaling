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

    // Normal Dungeon: Ceiling 5, Floor 3
    input = BaseInput();
    input.playerLevel = 20;
    input.instanceMaxLevel = 21;
    input.creatureMinLevel = 18;
    input.creatureSourceLevel = 21;
    input.observedCreatureLevel = 21;
    input.ceiling = 5;
    input.floor = 3;
    assert(Resolve(input) == 25); // Boss: 20 + 5 = 25

    input.creatureSourceLevel = 18;
    input.observedCreatureLevel = 18;
    assert(Resolve(input) == 22); // Trash: 20 + 5 - (21-18) = 22

    input.creatureSourceLevel = 10;
    input.observedCreatureLevel = 10;
    assert(Resolve(input) == 17); // Low trash: raw = 14, clamped to floor 20 - 3 = 17

    // Raid: Ceiling 3, Floor 0 (Zero downscaling below player level)
    input = BaseInput();
    input.playerLevel = 60;
    input.instanceMaxLevel = 63;
    input.creatureMinLevel = 60;
    input.creatureSourceLevel = 63;
    input.observedCreatureLevel = 63;
    input.ceiling = 3;
    input.floor = 0;
    assert(Resolve(input) == 63); // Boss: 60 + 3 = 63

    input.creatureSourceLevel = 58;
    input.observedCreatureLevel = 58;
    assert(Resolve(input) == 60); // Trash: raw = 58, clamped to floor 60 - 0 = 60

    // Heroic Dungeon (TBC & Wrath): Ceiling 5, Floor 0 (Zero downscaling below player level)
    input = BaseInput();
    input.playerLevel = 70;
    input.instanceMaxLevel = 72;
    input.creatureMinLevel = 68;
    input.creatureSourceLevel = 72;
    input.observedCreatureLevel = 72;
    input.ceiling = 5;
    input.floor = 0;
    assert(Resolve(input) == 75); // Boss: 70 + 5 = 75

    input.creatureSourceLevel = 68;
    input.observedCreatureLevel = 68;
    assert(Resolve(input) == 71); // Trash: 70 + 5 - (72-68) = 71

    input.creatureSourceLevel = 60;
    input.observedCreatureLevel = 60;
    assert(Resolve(input) == 70); // Deep trash: raw = 63, clamped to floor 70 - 0 = 70

    // Max Level (80) Heroic/Raid clamped at MaxLevel 80
    input = BaseInput();
    input.playerLevel = 80;
    input.instanceMaxLevel = 82;
    input.creatureMinLevel = 80;
    input.creatureSourceLevel = 82;
    input.observedCreatureLevel = 82;
    input.ceiling = 5;
    input.floor = 0;
    assert(Resolve(input) == 80); // Boss: 80 + 5 clamped to maxLevel 80
}
