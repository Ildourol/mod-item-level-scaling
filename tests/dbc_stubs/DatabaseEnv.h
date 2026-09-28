#ifndef ITEM_SCALING_TEST_DATABASE_ENV_H
#define ITEM_SCALING_TEST_DATABASE_ENV_H

#include "QueryResult.h"
#include <cassert>

struct FixtureDatabase
{
    std::vector<FixtureResult::Row> rows;
    bool failCount = false;
    bool failCoreOverlay = false;
    bool failVerification = false;

    QueryResult Query(std::string const& sql) const
    {
        if (sql == "SELECT COUNT(*) FROM item_dbc")
        {
            if (failCount)
                return {};
            FixtureResult::Row count{};
            count[0].value = std::to_string(rows.size());
            return std::make_shared<FixtureResult>(FixtureResult{{count}, 0});
        }
        if (sql == "SELECT * FROM `item_dbc` ORDER BY `ID` DESC")
        {
            if (failCoreOverlay)
                return {};
        }
        else
        {
            assert(sql == "SELECT ID,ClassID,SubclassID,Sound_Override_Subclassid,Material,DisplayInfoID,InventoryType,SheatheType "
                "FROM item_dbc ORDER BY ID DESC");
            if (failVerification)
                return {};
        }
        if (rows.empty())
            return {};
        return std::make_shared<FixtureResult>(FixtureResult{rows, 0});
    }
};

inline FixtureDatabase WorldDatabase;

#endif
