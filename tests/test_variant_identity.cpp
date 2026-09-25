#include "ItemScalingCommon.h"
#include <cassert>
#include <map>
#include <unordered_map>

int main()
{
    VariantKey first{100, 53, 60, 1, ITEM_SCALING_GENERATOR_REVISION, 50};
    VariantKey second{100, 53, 60, 1, ITEM_SCALING_GENERATOR_REVISION, 53};
    VariantKey otherFormula{100, 53, 60, 2, ITEM_SCALING_GENERATOR_REVISION, 50};
    VariantKey otherGenerator{100, 53, 60, 1,
        static_cast<uint8>(ITEM_SCALING_GENERATOR_REVISION + 1), 50};

    assert(!(first == second));
    assert(!(first == otherFormula));
    assert(!(first == otherGenerator));

    std::map<VariantKey, unsigned> entries;
    entries[first] = 60000;
    entries[second] = 60001;
    entries[otherFormula] = 60002;
    entries[otherGenerator] = 60003;

    assert(entries.size() == 4);
    assert(entries.at(first) == 60000);
    assert(entries.at(second) == 60001);
    assert(entries.at(otherGenerator) == 60003);

    std::unordered_map<VariantKey, unsigned, VariantKeyHash> lookup;
    lookup[first] = 60000;
    lookup[second] = 60001;
    lookup[otherFormula] = 60002;
    lookup[otherGenerator] = 60003;

    assert(lookup.size() == 4);
    assert(lookup.at(first) == 60000);
    assert(lookup.at(second) == 60001);
    assert(lookup.at(otherFormula) == 60002);
    assert(lookup.at(otherGenerator) == 60003);
}
