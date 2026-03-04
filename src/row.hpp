#pragma once

#include "common.hpp"
#include "page.hpp"
#include "value.hpp"

#include <algorithm>
#include <vector>

namespace row
{
struct ColumnPrefix
{
    page::Offset offset;
    page::Offset size;
};

struct Prefix
{
    page::Offset              size;
    std::vector<ColumnPrefix> columns;
};

[[nodiscard]] Prefix CalculateLayout(const Value& value);

void                Write(const Prefix& prefix, const Value& value, U8* row);
[[nodiscard]] Value Read(const Type& type, const U8* row);

[[nodiscard]] int Compare(const Type& type, ColumnId column, const U8* row_l, const U8* row_r);
[[nodiscard]] int Compare(const Type& type, ColumnId column, const U8* row_l, const Value& row_r);
} // namespace row
