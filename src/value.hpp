#pragma once

#include "common.hpp"
#include "type.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

enum class Bool : std::uint8_t
{
    kTrue,
    kFalse,
    kUnknown,
};

using ColumnValueNull    = std::monostate;
using ColumnValueBoolean = Bool;
using ColumnValueInteger = std::int64_t; // TODO: javascript i63
using ColumnValueReal    = double;
using ColumnValueVarchar = std::string;

inline int CompareStrings(std::string_view a, std::string_view b)
{
    const auto size_min = std::min(a.size(), b.size());
    const int  result   = memcmp(a.data(), b.data(), size_min);
    if (result != 0)
    {
        return result;
    }
    if (a.size() < b.size())
    {
        return -1;
    }
    if (a.size() > b.size())
    {
        return +1;
    }
    return 0;
}

using ColumnValue = std::variant<ColumnValueNull, ColumnValueBoolean, ColumnValueInteger,
                                 ColumnValueReal, ColumnValueVarchar>;

std::optional<ColumnType> ColumnValueToType(const ColumnValue& value);
std::string               ColumnValueToString(const ColumnValue& value, bool quote);
ColumnValue               ColumnValueEvalCast(const ColumnValue& value, ColumnType to);

using Value = std::vector<ColumnValue>;

void        ValuePrint(const Value& value);
std::string ValueToList(const Value& value);
bool        ValueEqual(const Value& a, const Value& b);
