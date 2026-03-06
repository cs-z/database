#include "row.hpp"
#include "common.hpp"
#include "page.hpp"
#include "type.hpp"
#include "value.hpp"

#include <cstring>
#include <utility>
#include <variant>

namespace row
{
Prefix CalculateLayout(const Value& value)
{
    Prefix prefix;
    prefix.size = value.size() * sizeof(ColumnPrefix);
    for (const ColumnValue& column_value : value)
    {
        const auto [column_align, column_size] = std::visit(
            Overload{
                [](const ColumnValueNull&)
                {
                    const page::Offset column_align = 1;
                    const page::Offset column_size  = 0;
                    return std::make_pair(column_align, column_size);
                },
                [](const ColumnValueBoolean&)
                {
                    const page::Offset column_align = alignof(ColumnValueBoolean);
                    const page::Offset column_size  = sizeof(ColumnValueBoolean);
                    return std::make_pair(column_align, column_size);
                },
                [](const ColumnValueInteger&)
                {
                    const page::Offset column_align = alignof(ColumnValueInteger);
                    const page::Offset column_size  = sizeof(ColumnValueInteger);
                    return std::make_pair(column_align, column_size);
                },
                [](const ColumnValueReal&)
                {
                    const page::Offset column_align = alignof(ColumnValueReal);
                    const page::Offset column_size  = sizeof(ColumnValueReal);
                    return std::make_pair(column_align, column_size);
                },
                [](const ColumnValueVarchar& column_value)
                {
                    const page::Offset column_align = alignof(char);
                    const page::Offset column_size  = column_value.size();
                    return std::make_pair(column_align, column_size);
                },
            },
            column_value);
        prefix.size = AlignUp(prefix.size, column_align);
        prefix.columns.push_back(
            {.offset = column_size != 0 ? prefix.size : page::Offset{}, .size = column_size});
        prefix.size += column_size;
    }
    return prefix;
}

template <typename T> static T* GetColumn(U8* row, ColumnId column)
{
    const page::Offset offset = reinterpret_cast<ColumnPrefix*>(row)[column.Get()].offset;
    return reinterpret_cast<T*>(row + offset);
}

void Write(const Prefix& prefix, const Value& value, U8* row)
{
    ASSERT(prefix.columns.size() == value.size());
    std::memcpy(row, prefix.columns.data(), prefix.columns.size() * sizeof(ColumnPrefix));
    for (ColumnId column_id{}; column_id < value.size(); column_id++)
    {
        std::visit(
            Overload{
                [](const ColumnValueNull&) {},
                [row, column_id](const ColumnValueBoolean& value)
                { *GetColumn<ColumnValueBoolean>(row, column_id) = value; },
                [row, column_id](const ColumnValueInteger& value)
                { *GetColumn<ColumnValueInteger>(row, column_id) = value; },
                [row, column_id](const ColumnValueReal& value)
                { *GetColumn<ColumnValueReal>(row, column_id) = value; },
                [row, column_id](const ColumnValueVarchar& value)
                {
                    // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
                    std::memcpy(GetColumn<char>(row, column_id), value.data(), value.size());
                },
            },
            value[column_id.Get()]);
    }
}

static ColumnPrefix GetPrefix(const U8* row, ColumnId column)
{
    return reinterpret_cast<const ColumnPrefix*>(row)[column.Get()];
}

template <typename T> static const T* GetColumn(const U8* row, ColumnPrefix prefix)
{
    return reinterpret_cast<const T*>(row + prefix.offset);
}

Value Read(const Type& type, const U8* row)
{
    Value value; // TODO: reserve
    for (ColumnId column_id{}; column_id < type.Size(); column_id++)
    {
        const ColumnPrefix prefix = GetPrefix(row, column_id);
        if (prefix.offset == 0)
        {
            value.emplace_back(ColumnValueNull{});
            continue;
        }
        switch (type.At(column_id.Get()))
        {
        case ColumnType::kBoolean:
        {
            value.emplace_back(*GetColumn<ColumnValueBoolean>(row, prefix));
            break;
        }
        case ColumnType::kInteger:
        {
            value.emplace_back(*GetColumn<ColumnValueInteger>(row, prefix));
            break;
        }
        case ColumnType::kReal:
        {
            value.emplace_back(*GetColumn<ColumnValueReal>(row, prefix));
            break;
        }
        case ColumnType::kVarchar:
        {
            const char* const begin = GetColumn<char>(row, prefix);
            value.emplace_back(ColumnValueVarchar{begin, prefix.size});
            break;
        }
        }
    }
    return value;
}

int Compare(const Type& type, ColumnId column, const U8* row_l, const U8* row_r)
{
    const ColumnPrefix prefix_l = GetPrefix(row_l, column);
    const ColumnPrefix prefix_r = GetPrefix(row_r, column);
    if (prefix_r.offset == 0)
    {
        return -1;
    }
    if (prefix_l.offset == 0)
    {
        return +1;
    }
    switch (type.At(column.Get()))
    {
    case ColumnType::kBoolean:
    {
        UNREACHABLE();
    }
    case ColumnType::kInteger:
    {
        const ColumnValueInteger column_value_l = *GetColumn<ColumnValueInteger>(row_l, prefix_l);
        const ColumnValueInteger column_value_r = *GetColumn<ColumnValueInteger>(row_r, prefix_r);
        if (column_value_l < column_value_r)
        {
            return -1;
        }
        if (column_value_l > column_value_r)
        {
            return +1;
        }
        return 0;
    }
    case ColumnType::kReal:
    {
        const ColumnValueReal column_value_l = *GetColumn<ColumnValueReal>(row_l, prefix_l);
        const ColumnValueReal column_value_r = *GetColumn<ColumnValueReal>(row_r, prefix_r);
        if (column_value_l < column_value_r)
        {
            return -1;
        }
        if (column_value_l > column_value_r)
        {
            return +1;
        }
        return 0;
    }
    case ColumnType::kVarchar:
    {
        const char* column_value_l = GetColumn<char>(row_l, prefix_l);
        const char* column_value_r = GetColumn<char>(row_r, prefix_r);
        return CompareStrings({column_value_l, prefix_l.size}, {column_value_r, prefix_r.size});
    }
    }
    UNREACHABLE();
}

int Compare(const Type& type, ColumnId column, const U8* row_l, const Value& row_r)
{
    const ColumnPrefix prefix_l = GetPrefix(row_l, column);
    const ColumnValue& value_r  = row_r.at(column.Get());
    if (std::holds_alternative<ColumnValueNull>(value_r))
    {
        return -1;
    }
    if (prefix_l.offset == 0)
    {
        return +1;
    }
    switch (type.At(column.Get()))
    {
    case ColumnType::kBoolean:
    {
        UNREACHABLE();
    }
    case ColumnType::kInteger:
    {
        const ColumnValueInteger column_value_l = *GetColumn<ColumnValueInteger>(row_l, prefix_l);
        const ColumnValueInteger column_value_r = std::get<ColumnValueInteger>(value_r);
        if (column_value_l < column_value_r)
        {
            return -1;
        }
        if (column_value_l > column_value_r)
        {
            return +1;
        }
        return 0;
    }
    case ColumnType::kReal:
    {
        const ColumnValueReal column_value_l = *GetColumn<ColumnValueReal>(row_l, prefix_l);
        const ColumnValueReal column_value_r = std::get<ColumnValueReal>(value_r);
        if (column_value_l < column_value_r)
        {
            return -1;
        }
        if (column_value_l > column_value_r)
        {
            return +1;
        }
        return 0;
    }
    case ColumnType::kVarchar:
    {
        const char* column_value_l = GetColumn<char>(row_l, prefix_l);
        return CompareStrings({column_value_l, prefix_l.size},
                              std::get<ColumnValueVarchar>(value_r));
    }
    }
    UNREACHABLE();
}
} // namespace row