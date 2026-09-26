#include "ItemScalingCommon.h"
#include <cassert>
#include <map>
#include <unordered_map>

int main()
{
    VariantKey first{100, 53, 60, 1, 50};
    VariantKey second{100, 53, 60, 1, 53};
    VariantKey otherFormula{100, 53, 60, 2, 50};

    assert(!(first == second));
    assert(!(first == otherFormula));

    std::map<VariantKey, unsigned> entries;
    entries[first] = 60000;
    entries[second] = 60001;
    entries[otherFormula] = 60002;

    assert(entries.size() == 3);
    assert(entries.at(first) == 60000);
    assert(entries.at(second) == 60001);
    assert(entries.at(otherFormula) == 60002);

    std::unordered_map<VariantKey, unsigned, VariantKeyHash> lookup;
    lookup[first] = 60000;
    lookup[second] = 60001;
    lookup[otherFormula] = 60002;

    assert(lookup.size() == 3);
    assert(lookup.at(first) == 60000);
    assert(lookup.at(second) == 60001);
    assert(lookup.at(otherFormula) == 60002);
}
