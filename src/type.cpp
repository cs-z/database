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
    case ColumnType::kBoolean:
        return "BOOLEAN";
    case ColumnType::kInteger:
        return "INTEGER";
    case ColumnType::kReal:
        return "REAL";
    case ColumnType::kVarchar:
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
        case ColumnType::kBoolean:
            switch (to.first)
            {
            case ColumnType::kInteger:
            case ColumnType::kReal:
                break;
            case ColumnType::kBoolean:
            case ColumnType::kVarchar:
                return;
            }
            break;
        case ColumnType::kInteger:
        case ColumnType::kReal:
            switch (to.first)
            {
            case ColumnType::kBoolean:
                break;
            case ColumnType::kInteger:
            case ColumnType::kReal:
            case ColumnType::kVarchar:
                return;
            }
            break;
        case ColumnType::kVarchar:
            switch (to.first)
            {
            case ColumnType::kBoolean:
            case ColumnType::kInteger:
            case ColumnType::kReal:
                break;
            case ColumnType::kVarchar:
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
    case ColumnType::kBoolean:
        align_ = std::max<page::Offset>(align_, alignof(ColumnValueBoolean));
        break;
    case ColumnType::kInteger:
        align_ = std::max<page::Offset>(align_, alignof(ColumnValueInteger));
        break;
    case ColumnType::kReal:
        align_ = std::max<page::Offset>(align_, alignof(ColumnValueReal));
        break;
    case ColumnType::kVarchar:
        align_ = std::max<page::Offset>(align_, alignof(char));
        break;
    }
    columns_.push_back(column);
}
