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

std::string ColumnTypeToString(ColumnType type)
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

std::string ColumnTypeToCatalogString(ColumnType type)
{
    return ColumnTypeToString(type);
}

ColumnType ColumnTypeFromCatalogString(const std::string& name)
{
    return ParseType(name);
}

void CompileCast(std::optional<ColumnType> from, const std::pair<ColumnType, SourceText>& to)
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
    throw ClientError{"invalid cast from " + (from ? ColumnTypeToString(*from) : "NULL") + " to " +
                          ColumnTypeToString(to.first),
                      to.second};
}

Type::Type() : align_{alignof(row::ColumnPrefix)}
{
}

void Type::Push(ColumnType column)
{
    switch (column)
    {
    case ColumnType::BOOLEAN:
        align_ = std::max<page::Offset>(align_, alignof(ColumnValueBoolean));
        break;
    case ColumnType::INTEGER:
        align_ = std::max<page::Offset>(align_, alignof(ColumnValueInteger));
        break;
    case ColumnType::REAL:
        align_ = std::max<page::Offset>(align_, alignof(ColumnValueReal));
        break;
    case ColumnType::VARCHAR:
        align_ = std::max<page::Offset>(align_, alignof(char));
        break;
    }
    columns_.push_back(column);
}
