// Numeric database transport fixture for the isolated, unmodified core DBC loader.
#ifndef ITEM_SCALING_TEST_QUERY_RESULT_H
#define ITEM_SCALING_TEST_QUERY_RESULT_H

#include "Define.h"
#include <array>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

struct Field
{
    std::string value;

    template <typename T> T Get() const
    {
        if constexpr (std::is_same_v<T, std::string>)
            return value;
        else
            return static_cast<T>(std::stoll(value));
    }
};

struct FixtureResult
{
    using Row = std::array<Field, 8>;
    std::vector<Row> rows;
    std::size_t position = 0;

    Field* Fetch() { return rows.at(position).data(); }
    Field const& operator[](std::size_t index) const { return rows.at(position).at(index); }
    uint32 GetFieldCount() const { return 8; }
    uint64 GetRowCount() const { return rows.size(); }
    bool NextRow() { return ++position < rows.size(); }
};

using QueryResult = std::shared_ptr<FixtureResult>;

#endif
