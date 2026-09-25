#include "ItemScalingSafety.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace ItemScalingSafety;
    for (unsigned low = 0; low < 1024; ++low)
    {
        for (unsigned high = 0; high < 1024; ++high)
        {
            auto [minimum, maximum] = LevelRange(low, high);
            assert(minimum >= 1 && maximum <= 80 && minimum <= maximum);
        }
    }
    auto [minimum, maximum] = LevelRange(255, 80);
    assert(minimum == 80 && maximum == 80); // Previously became [80,255] and wrapped forever.
    for (unsigned low = 1; low <= 80; ++low)
    {
        for (unsigned high = low; high <= 80; ++high)
        {
            for (unsigned step = 1; step <= 10; ++step)
            {
                for (unsigned level = low; level <= high; ++level)
                {
                    auto target = Bracket(level, low, high, step);
                    assert(target >= low && target <= level);
                    assert(Bracket(target, low, high, step) == target);
                    if (level == high)
                        assert(target == high);
                }
            }
        }
    }
    assert(AllocationStart(54806, 0, true, 0, 1000) == 60000);
    assert(AllocationStart(75000, 70000, true, 0, 1000) == 75001);
    assert(AllocationStart(75000, 80000, true, 0, 1000) == 80001);
    assert(AllocationStart(75000, 0, false, 60000, 1000) == 75001);
    assert(AllocationStart(75000, 0, false, 90000, 1000) == 90000);
    auto maximumId = std::numeric_limits<std::uint32_t>::max();
    assert(AllocationStart(maximumId, maximumId, true, 0, 1000) == std::uint64_t{maximumId} + 1);
}
