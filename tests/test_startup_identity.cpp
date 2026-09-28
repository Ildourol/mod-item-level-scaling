#include "DatabaseEnv.h"
#include "ItemScalingStartupIdentity.h"
#include "DBCfmt.h"
#include <cassert>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <iostream>

// Supply assertion reporting without linking/configuring the full core.
std::string GetDebugInfo() { return {}; }
namespace Acore
{
    [[noreturn]] void Assert(std::string_view, uint32, std::string_view, std::string_view,
        std::string_view, std::string_view)
    {
        std::abort();
    }
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string const path = std::string(argv[1]) + "/Item.dbc";
    // A real WDBC file read by the core's DBCFileLoader/DBCStorage, plus numeric SQL transport fixtures.
    uint32 const header[]{0x43424457, 1, 8, 32, 1};
    ItemEntry const row{100, 4, 6, -1, -1, 12345, 14, 1};
    static_assert(sizeof(row) == 32);
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<char const*>(header), sizeof(header));
        file.write(reinterpret_cast<char const*>(&row), sizeof(row));
        file.put('\0');
        assert(file.good());
    }
    ItemTemplate raw{};
    raw.ItemId = 100;
    raw.Class = 2;
    raw.InventoryType = 13;
    raw.DisplayInfoID = 999;
    auto const saved = ItemScalingIdentity::FromDBC(row);
    {
        DBCStorage<ItemEntry> items(Itemfmt);
        assert(ItemScalingStartupIdentity::Load(items, path)); // Empty overlay preserves file data.
        assert(ItemScalingIdentity::Resolve(raw, items.LookupEntry(100), true) == saved);
        assert(ItemScalingIdentity::Resolve(raw, items.LookupEntry(100), false) != saved);
        assert(ItemScalingIdentity::Resolve(raw, items.LookupEntry(999), true) == ItemScalingIdentity::Capture(raw));
    }
    WorldDatabase.rows = {
        {{{"200"}, {"2"}, {"7"}, {"-1"}, {"-1"}, {"45678"}, {"13"}, {"3"}}},
        {{{"100"}, {"4"}, {"6"}, {"-1"}, {"-1"}, {"12346"}, {"14"}, {"1"}}}
    };
    {
        DBCStorage<ItemEntry> nextStart(Itemfmt);
        assert(ItemScalingStartupIdentity::Load(nextStart, path));
        assert(nextStart.LookupEntry(200)); // DB extends the file's entry range.
        assert(nextStart.LookupEntry(100)->DisplayInfoID == 12346); // DB wins over file.
        assert(ItemScalingIdentity::Resolve(raw, nextStart.LookupEntry(100), true) != saved);
        assert(nextStart.LookupEntry(200)->SoundOverrideSubclassID == -1);
        assert(nextStart.LookupEntry(200)->Material == -1);
    }
    {
        DBCStorage<ItemEntry> databaseOnly(Itemfmt);
        assert(ItemScalingStartupIdentity::Load(databaseOnly, path + ".absent"));
        assert(databaseOnly.LookupEntry(100)->DisplayInfoID == 12346);
        auto const updated = ItemScalingIdentity::Resolve(raw, databaseOnly.LookupEntry(100), true);
        assert(updated != saved);
        ItemTemplate loaded = raw;
        updated.Apply(loaded);
        assert(updated.Matches(loaded)); // A newly captured request can match the following run.
    }
    for (bool* failure : {&WorldDatabase.failCount, &WorldDatabase.failCoreOverlay, &WorldDatabase.failVerification})
    {
        *failure = true;
        DBCStorage<ItemEntry> failed(Itemfmt);
        assert(!ItemScalingStartupIdentity::Load(failed, path));
        *failure = false;
    }
    WorldDatabase.rows.clear();
    DBCStorage<ItemEntry> unavailable(Itemfmt);
    assert(!ItemScalingStartupIdentity::Load(unavailable, path + ".absent"));
    assert(unavailable.GetNumRows() == 0);

    // Representative loader-only timing. Numeric DB transport is in memory, not a DB round trip.
    for (uint32 id = 50000; id > 0; --id)
        WorldDatabase.rows.push_back({{{std::to_string(id)}, {"4"}, {"6"}, {"-1"}, {"-1"},
            {"12345"}, {"14"}, {"1"}}});
    auto const started = std::chrono::steady_clock::now();
    DBCStorage<ItemEntry> many(Itemfmt);
    assert(ItemScalingStartupIdentity::Load(many, path));
    assert(many.LookupEntry(50000) && many.LookupEntry(1));
    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    std::cout << "50,000-row private identity load/verification: " << elapsed << " ms (fixture DB transport)\n";
}
