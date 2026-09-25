#include "ItemScalingCommon.h"
#include <cassert>
#include <map>

int main()
{
    VariantKey first{100, 53, 60, 1, 50};
    VariantKey second{100, 53, 60, 1, 53};
    VariantKey otherVersion{100, 53, 60, 2, 50};
    assert(!(first == second));
    std::map<VariantKey, unsigned> entries;
    entries[first] = 60000;
    entries[second] = 60001;
    entries[otherVersion] = 60002;
    assert(entries.size() == 3);
    assert(entries.at(first) == 60000);
    assert(entries.at(second) == 60001);
}
