#include "ItemScalingTarget.h"
#include "ItemScalingVariance.h"
#include <cassert>

namespace
{
    ItemScalingTarget::Input BaseInput()
    {
        ItemScalingTarget::Input input;
        input.playerLevel = 80;
        input.creatureMinLevel = 80;
        input.creatureSourceLevel = 83;
        input.instanceMaxLevel = 83;
        input.observedCreatureLevel = 83;
        input.floor = 3;
        input.ceiling = 0;
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

    // Default BaseInput: Player 80, Raid Boss 83, Floor 3, Ceiling 0 -> 80
    auto input = BaseInput();
    assert(Resolve(input) == 80);

    // IsExternallyScaledCreature helper tests
    input = BaseInput();
    input.creatureMinLevel = 70;
    input.creatureSourceLevel = 73;
    input.observedCreatureLevel = 83; // scaled up by AutoBalance
    assert(IsExternallyScaledCreature(input));

    input.observedCreatureLevel = 73; // native unscaled
    assert(!IsExternallyScaledCreature(input));

    input.hasCreature = false;
    assert(!IsExternallyScaledCreature(input));

    // Fixed Scaling Mode: dynamic = false -> returns playerLevel
    input = BaseInput();
    input.dynamic = false;
    input.observedCreatureLevel = 83;
    assert(Resolve(input) == 80);

    // Chests / GameObjects: hasCreature = false -> returns playerLevel
    input = BaseInput();
    input.hasCreature = false;
    assert(Resolve(input) == 80);

    // =========================================================================
    // Core Feature: Scaling Based on Current In-Instance Mob Level (Default Floor = 3)
    // =========================================================================

    // 1. Raid Boss (e.g. Void Reaver, Ragnaros, Illidan scaled to 83 at Player 80)
    // Floor = 3 (Default): 83 - 3 = 80
    input = BaseInput();
    input.playerLevel = 80;
    input.observedCreatureLevel = 83;
    input.floor = 3;
    input.ceiling = 0;
    assert(Resolve(input) == 80);

    // Floor = 5: 83 - 5 = 78
    input.floor = 5;
    assert(Resolve(input) == 78);

    // Floor = 0: 83 - 0 = 83, clamped to PlayerLevel (80) + Ceiling (0) = 80
    input.floor = 0;
    assert(Resolve(input) == 80);

    // Floor = 10: 83 - 10 = 73
    input.floor = 10;
    assert(Resolve(input) == 73);

    // 2. Raid Trash Mobs (scaled to 80 at Player 80)
    // Floor = 3 (Default): 80 - 3 = 77
    input = BaseInput();
    input.playerLevel = 80;
    input.observedCreatureLevel = 80;
    input.floor = 3;
    input.ceiling = 0;
    assert(Resolve(input) == 77);

    // Floor = 5: 80 - 5 = 75
    input.floor = 5;
    assert(Resolve(input) == 75);

    // Floor = 0: 80 - 0 = 80
    input.floor = 0;
    assert(Resolve(input) == 80);

    // 3. 5-Man Dungeon Boss (e.g. Botanica Boss scaled to 82 at Player 80)
    // Floor = 3 (Default): 82 - 3 = 79
    input = BaseInput();
    input.playerLevel = 80;
    input.observedCreatureLevel = 82;
    input.floor = 3;
    input.ceiling = 0;
    assert(Resolve(input) == 79);

    // Floor = 2: 82 - 2 = 80
    input.floor = 2;
    assert(Resolve(input) == 80);

    // Floor = 5: 82 - 5 = 77
    input.floor = 5;
    assert(Resolve(input) == 77);

    // Floor = 0: 82 - 0 = 82, clamped to 80
    input.floor = 0;
    assert(Resolve(input) == 80);

    // 4. 5-Man Dungeon Trash (scaled to 80 at Player 80)
    input = BaseInput();
    input.playerLevel = 80;
    input.observedCreatureLevel = 80;
    input.floor = 3;
    assert(Resolve(input) == 77);
    input.floor = 0;
    assert(Resolve(input) == 80);

    // 5. Level 70 Player in Tempest Keep / Black Temple (Native Raid Boss 73)
    // Floor = 3 (Default): 73 - 3 = 70 (Drops Level 70 item -> triggers native match bypass!)
    input = BaseInput();
    input.playerLevel = 70;
    input.observedCreatureLevel = 73;
    input.floor = 3;
    input.ceiling = 0;
    assert(Resolve(input) == 70);

    // Floor = 5: 73 - 5 = 68
    input.floor = 5;
    assert(Resolve(input) == 68);

    // Floor = 0: 73 - 0 = 73, clamped to PlayerLevel (70) + Ceiling (0) = 70
    input.floor = 0;
    assert(Resolve(input) == 70);

    // Level 70 Player with Ceiling = 2 (allows up to level 72 gear)
    input.ceiling = 2;
    input.floor = 0; // 73 clamped to 70 + 2 = 72
    assert(Resolve(input) == 72);
    input.floor = 1; // 73 - 1 = 72
    assert(Resolve(input) == 72);
    input.floor = 3; // 73 - 3 = 70
    assert(Resolve(input) == 70);

    // 6. Level 60 Player in Molten Core (Boss scaled to 63)
    input = BaseInput();
    input.playerLevel = 60;
    input.observedCreatureLevel = 63;
    input.floor = 3;
    input.ceiling = 0;
    assert(Resolve(input) == 60); // 63 - 3 = 60

    input.observedCreatureLevel = 60; // Trash: 60 - 3 = 57
    assert(Resolve(input) == 57);

    // 7. Level 20 Player in Deadmines (VanCleef scaled to 22, Trash at 20)
    input = BaseInput();
    input.playerLevel = 20;
    input.observedCreatureLevel = 22;
    input.floor = 2;
    assert(Resolve(input) == 20); // 22 - 2 = 20
    input.floor = 3;
    assert(Resolve(input) == 19); // 22 - 3 = 19

    input.observedCreatureLevel = 20; // Trash: 20 - 3 = 17
    assert(Resolve(input) == 17);

    // Boundary & Clamping Checks
    input = BaseInput();
    input.playerLevel = 1;
    input.observedCreatureLevel = 1;
    input.floor = 5;
    input.minLevel = 1;
    assert(Resolve(input) == 1); // clamped to minLevel 1

    input = BaseInput();
    input.playerLevel = 80;
    input.observedCreatureLevel = 90;
    input.ceiling = 5;
    input.maxLevel = 80;
    input.floor = 0;
    assert(Resolve(input) == 80); // clamped to maxLevel 80

    // =========================================================================
    // Native Reference Level and Native Target Match Acceptance Tests
    // =========================================================================
    using ItemScalingTarget::GetNativeReferenceLevel;
    using ItemScalingTarget::IsNativeTargetMatch;

    // GetNativeReferenceLevel tests
    assert(GetNativeReferenceLevel(70, 151) == 70); // RequiredLevel takes precedence
    assert(GetNativeReferenceLevel(0, 25) == 25);   // Fallback to ItemLevel when RequiredLevel is 0
    assert(GetNativeReferenceLevel(0, 0) == 1);     // Clamped min 1
    assert(GetNativeReferenceLevel(0, 245) == 80);  // Clamped max 80

    // Acceptance Case 1: H=70, BT native item=70, ceiling=0, target=70 -> original item, 0 synthetic generation
    assert(IsNativeTargetMatch(70, 70, 70, 70) == true);

    // Acceptance Case 2: H=80, BT native item=70, target=80 -> generate scaled variant
    assert(IsNativeTargetMatch(70, 80, 80, 80) == false);

    // Acceptance Case 3: H=60, MC native item=60, target=60 -> original item
    assert(IsNativeTargetMatch(60, 60, 60, 60) == true);

    // Acceptance Case 4: H=70, MC native item=60, target=70 -> generate scaled variant
    assert(IsNativeTargetMatch(60, 70, 70, 70) == false);

    // Acceptance Case 5: H=70, native item=80, target=70 -> normal downscaling allowed
    assert(IsNativeTargetMatch(80, 70, 70, 70) == false);

    // Acceptance Case 6: native=70, raw target=71, BracketStep causes effective target=70 -> preserve original
    assert(IsNativeTargetMatch(70, 71, 70, 71) == true);

    // Acceptance Case 7: Lower-level dungeon: H=20, Deadmines boss drop native=20, target=20 -> original item
    assert(IsNativeTargetMatch(20, 20, 20, 20) == true);

    // Acceptance Case 8: Lower-level dungeon: H=20, Deadmines mob drop native=18, mob target=18 -> original item
    assert(IsNativeTargetMatch(18, 18, 18, 20) == true);

    // Acceptance Case 9: Lower-level dungeon: H=40, Scarlet Monastery Herod drop native=40, target=40 -> original item
    assert(IsNativeTargetMatch(40, 40, 40, 40) == true);

    // Acceptance Case 10: Lower-level dungeon trash drop native=40, player=40 -> native match
    assert(IsNativeTargetMatch(40, 38, 38, 40) == true);

    // Acceptance Case 11: Diverse boss levels (BRD): H=52, Gerstahn native=47, target=47 -> original item
    assert(IsNativeTargetMatch(47, 47, 47, 52) == true);

    // Acceptance Case 12: Diverse boss levels (BRD): H=52, Gerstahn native=52, player=52 -> original item
    assert(IsNativeTargetMatch(52, 47, 47, 52) == true);

    // Diverse boss levels: Level 80 player in BRD -> native items must NOT match, scaling up proceeds
    assert(IsNativeTargetMatch(47, 75, 75, 80) == false);
    assert(IsNativeTargetMatch(52, 75, 75, 80) == false);

    // =========================================================================
    // Feature: Dynamic Floor & Ceiling Variance (Weights, Rolling, Target Calculation)
    // =========================================================================

    // 1. Parsing tests
    {
        // Default format: "-1:20.0, -2:10.0, -3:5.0"
        auto w = ItemScalingVariance::ParseWeights("-1:20.0, -2:10.0, -3:5.0");
        assert(w.HasAny());
        assert(w.chances[0] == 5.0f);  // -3
        assert(w.chances[1] == 10.0f); // -2
        assert(w.chances[2] == 20.0f); // -1
        assert(w.chances[3] == 0.0f);  // +1
        assert(w.chances[4] == 0.0f);  // +2
        assert(w.chances[5] == 0.0f);  // +3
        assert(w.TotalWeight() == 35.0f);

        // Positional format: "5.0, 10.0, 20.0, 0.0, 0.0, 0.0"
        auto wPos = ItemScalingVariance::ParseWeights("5.0, 10.0, 20.0, 0.0, 0.0, 0.0");
        assert(wPos.chances[0] == 5.0f);
        assert(wPos.chances[1] == 10.0f);
        assert(wPos.chances[2] == 20.0f);

        // Empty string
        auto wEmpty = ItemScalingVariance::ParseWeights("");
        assert(!wEmpty.HasAny());
        assert(wEmpty.TotalWeight() == 0.0f);

        // Single delta format: "-1: 25.0"
        auto wSingle = ItemScalingVariance::ParseWeights("-1: 25.0");
        assert(wSingle.chances[2] == 25.0f);
        assert(wSingle.TotalWeight() == 25.0f);

        // Ceiling deltas: "+1:10.0, +2:5.0"
        auto wCeil = ItemScalingVariance::ParseWeights("+1:10.0, +2:5.0");
        assert(wCeil.chances[3] == 10.0f);
        assert(wCeil.chances[4] == 5.0f);
        assert(wCeil.TotalWeight() == 15.0f);
    }

    // 2. Rolling tests (deterministic rolls against default weights)
    {
        auto w = ItemScalingVariance::ParseWeights("-1:20.0, -2:10.0, -3:5.0");
        // Intervals: [-3]: 0.0..5.0, [-2]: 5.0..15.0, [-1]: 15.0..35.0, [0]: 35.0..100.0
        assert(ItemScalingVariance::RollDelta(w, 2.0f) == -3);
        assert(ItemScalingVariance::RollDelta(w, 4.99f) == -3);
        assert(ItemScalingVariance::RollDelta(w, 5.0f) == -2);
        assert(ItemScalingVariance::RollDelta(w, 14.99f) == -2);
        assert(ItemScalingVariance::RollDelta(w, 15.0f) == -1);
        assert(ItemScalingVariance::RollDelta(w, 34.99f) == -1);
        assert(ItemScalingVariance::RollDelta(w, 35.0f) == 0);
        assert(ItemScalingVariance::RollDelta(w, 75.0f) == 0);
        assert(ItemScalingVariance::RollDelta(w, 99.9f) == 0);

        // Empty weights always roll 0
        auto wEmpty = ItemScalingVariance::ParseWeights("");
        assert(ItemScalingVariance::RollDelta(wEmpty, 1.0f) == 0);
    }

    // 3. Target Level Resolution with Rolled Variance
    {
        // Case A: Level 80 player in Dungeon (Boss is level 82, Base Floor is 3)
        // Standard baseline roll (Delta 0): Floor 3 -> 82 - 3 = 79
        input = BaseInput();
        input.playerLevel = 80;
        input.observedCreatureLevel = 82;
        input.floor = 3;
        input.ceiling = 0;
        assert(Resolve(input) == 79);

        // Lucky roll (Delta -1): Floor becomes 2 -> 82 - 2 = 80!
        input.floor = 2;
        assert(Resolve(input) == 80);

        // Great roll (Delta -2): Floor becomes 1 -> 82 - 1 = 81, clamped to 80 by ceiling
        input.floor = 1;
        assert(Resolve(input) == 80);

        // Jackpot roll (Delta -3): Floor becomes 0 -> 82 - 0 = 82, clamped to 80 by ceiling
        input.floor = 0;
        assert(Resolve(input) == 80);

        // Case B: Level 80 player in Dungeon (Trash mob is level 80, Base Floor is 3)
        // Standard baseline roll (Delta 0): Floor 3 -> 80 - 3 = 77
        input.observedCreatureLevel = 80;
        input.floor = 3;
        assert(Resolve(input) == 77);

        // Delta -1: Floor 2 -> 80 - 2 = 78
        input.floor = 2;
        assert(Resolve(input) == 78);

        // Delta -2: Floor 1 -> 80 - 1 = 79
        input.floor = 1;
        assert(Resolve(input) == 79);

        // Delta -3 (Jackpot!): Floor 0 -> 80 - 0 = 80! Max level drop from trash mob!
        input.floor = 0;
        assert(Resolve(input) == 80);

        // Case C: Raid Boss with Ceiling Variance
        // Level 80 player, Raid Boss 83, Floor 2 (delta -1), Ceiling 1 (delta +1) -> 83 - 2 = 81 (Allowed!)
        input.observedCreatureLevel = 83;
        input.floor = 2;
        input.ceiling = 1;
        assert(Resolve(input) == 81);

        // Case D: Floor Clamping (cannot underflow below 0)
        // Base Floor 1 with Delta -3 -> clamped to 0 -> 80 - 0 = 80
        input.observedCreatureLevel = 80;
        uint8 effectiveFloor = static_cast<uint8>(std::clamp<int32>(1 + (-3), 0, 80));
        input.floor = effectiveFloor;
        input.ceiling = 0;
        assert(Resolve(input) == 80);
    }

    return 0;
}
