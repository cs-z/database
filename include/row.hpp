#pragma once

#include <algorithm>

#include "common.hpp"
#include "page.hpp"
#include "value.hpp"

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

Prefix calculate_layout(const Value& value);

void  write(const Prefix& prefix, const Value& value, u8* row);
Value read(const Type& type, const u8* row);

int compare(const Type& type, ColumnId column, const u8* row_l, const u8* row_r);
int compare(const Type& type, ColumnId column, const u8* row_l, const Value& row_r);
}  // namespace row
