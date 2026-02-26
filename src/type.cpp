#include "type.hpp"
#include "common.hpp"
#include "error.hpp"
#include "page.hpp"
#include "parse.hpp"
#include "row.hpp"
#include "value.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

std::string column_type_to_string(ColumnType type)
{
    switch (type)
    {
    case ColumnType::BOOLEAN:
        return "BOOLEAN";
    case ColumnType::INTEGER:
        return "INTEGER";
    case ColumnType::REAL:
        return "REAL";
    case ColumnType::VARCHAR:
        return "VARCHAR";
    }
    UNREACHABLE();
}

std::string column_type_to_catalog_string(ColumnType type)
{
    return column_type_to_string(type);
}

ColumnType column_type_from_catalog_string(const std::string& name)
{
    return parse_type(name);
}

void compile_cast(std::optional<ColumnType> from, const std::pair<ColumnType, SourceText>& to)
{
    if (from)
    {
        switch (*from)
        {
        case ColumnType::BOOLEAN:
            switch (to.first)
            {
            case ColumnType::INTEGER:
            case ColumnType::REAL:
                break;
            case ColumnType::BOOLEAN:
            case ColumnType::VARCHAR:
                return;
            }
            break;
        case ColumnType::INTEGER:
        case ColumnType::REAL:
            switch (to.first)
            {
            case ColumnType::BOOLEAN:
                break;
            case ColumnType::INTEGER:
            case ColumnType::REAL:
            case ColumnType::VARCHAR:
                return;
            }
            break;
        case ColumnType::VARCHAR:
            switch (to.first)
            {
            case ColumnType::BOOLEAN:
            case ColumnType::INTEGER:
            case ColumnType::REAL:
                break;
            case ColumnType::VARCHAR:
                return;
            }
            break;
        }
    }
    throw ClientError{"invalid cast from " + (from ? column_type_to_string(*from) : "NULL") +
                          " to " + column_type_to_string(to.first),
                      to.second};
}

Type::Type() : align{alignof(row::ColumnPrefix)}
{
}

void Type::push(ColumnType column)
{
    switch (column)
    {
    case ColumnType::BOOLEAN:
        align = std::max<page::Offset>(align, alignof(ColumnValueBoolean));
        break;
    case ColumnType::INTEGER:
        align = std::max<page::Offset>(align, alignof(ColumnValueInteger));
        break;
    case ColumnType::REAL:
        align = std::max<page::Offset>(align, alignof(ColumnValueReal));
        break;
    case ColumnType::VARCHAR:
        align = std::max<page::Offset>(align, alignof(char));
        break;
    }
    columns.push_back(column);
}
