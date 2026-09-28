/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE
 */

#ifndef _ITEM_SCALING_STARTUP_IDENTITY_H
#define _ITEM_SCALING_STARTUP_IDENTITY_H

#include "DatabaseEnv.h"
#include "ItemScalingIdentity.h"
#include "QueryResult.h"

namespace ItemScalingStartupIdentity
{
    inline bool Load(DBCStorage<ItemEntry>& items, std::string const& itemDbcPath)
    {
        // Same file-first, database-second order as LoadDBCStores. This store is private and short-lived.
        items.Load(itemDbcPath.c_str());
        QueryResult count = WorldDatabase.Query("SELECT COUNT(*) FROM item_dbc");
        if (!count)
            return false;
        items.LoadFromDB("item_dbc", items.GetFormat());

        // LoadFromDB has no status result. Verify the overlay rather than treating a failed read as empty.
        QueryResult overlay = WorldDatabase.Query(
            "SELECT ID,ClassID,SubclassID,Sound_Override_Subclassid,Material,DisplayInfoID,InventoryType,SheatheType "
            "FROM item_dbc ORDER BY ID DESC");
        if (count->Fetch()[0].Get<uint64>() != (overlay ? overlay->GetRowCount() : 0) || !items.GetNumRows())
            return false;
        if (overlay)
        {
            do
            {
                Field* fields = overlay->Fetch();
                ItemEntry const* loaded = items.LookupEntry(fields[0].Get<uint32>());
                ItemScalingIdentity const expected{fields[1].Get<uint32>(), fields[2].Get<uint32>(),
                    fields[3].Get<int32>(), fields[4].Get<int32>(), fields[5].Get<uint32>(),
                    fields[6].Get<uint32>(), fields[7].Get<uint32>()};
                if (!loaded || ItemScalingIdentity::FromDBC(*loaded) != expected)
                    return false;
            } while (overlay->NextRow());
        }
        return true;
    }
}

#endif // _ITEM_SCALING_STARTUP_IDENTITY_H
