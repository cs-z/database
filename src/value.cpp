#include "value.hpp"
#include "common.hpp"
#include "type.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <variant>

std::optional<ColumnType> ColumnValueToType(const ColumnValue& value)
{
    return std::visit(
        Overload{
            [](const ColumnValueNull&) -> std::optional<ColumnType> { return std::nullopt; },
            [](const ColumnValueBoolean&) -> std::optional<ColumnType>
            { return ColumnType::kBoolean; },
            [](const ColumnValueInteger&) -> std::optional<ColumnType>
            { return ColumnType::kInteger; },
            [](const ColumnValueReal&) -> std::optional<ColumnType> { return ColumnType::kReal; },
            [](const ColumnValueVarchar&) -> std::optional<ColumnType>
            { return ColumnType::kVarchar; },
        },
        value);
}

std::string ColumnValueToString(const ColumnValue& value, bool quote)
{
    return std::visit(
        Overload{
            [](const ColumnValueNull&) -> std::string { return "NULL"; },
            [](const ColumnValueBoolean& value) -> std::string
            {
                switch (value)
                {
                case Bool::kTrue:
                    return "TRUE";
                case Bool::kFalse:
                    return "FALSE";
                case Bool::kUnknown:
                    return "UNKNOWN";
                }
                UNREACHABLE();
            },
            [](const ColumnValueInteger& value) { return std::to_string(value); },
            [](const ColumnValueReal& value)
            {
                const std::string string = std::to_string(value);
                ASSERT(!string.empty());
                std::size_t ptr = string.size() - 1;
                while (ptr > 0 && string[ptr] == '0' && string[ptr - 1] == '0')
                {
                    ptr--;
                }
                if (ptr > 0 && string[ptr] == '0' && string[ptr - 1] != '.')
                {
                    ptr--;
                }
                return string.substr(0, ptr + 1);
            },
            [quote](const ColumnValueVarchar& value)
            { return quote ? ('\'' + value + '\'') : value; },
        },
        value);
}

ColumnValue ColumnValueEvalCast(const ColumnValue& value, ColumnType to)
{
    return std::visit(
        Overload{
            [](const ColumnValueNull& value) -> ColumnValue { return value; },
            [to](const ColumnValueBoolean& value) -> ColumnValue
            {
                switch (to)
                {
                case ColumnType::kInteger:
                case ColumnType::kReal:
                    UNREACHABLE();
                case ColumnType::kBoolean:
                    return value;
                case ColumnType::kVarchar:
                    return ColumnValueToString(value, false);
                }
                UNREACHABLE();
            },
            [to](const ColumnValueInteger& value) -> ColumnValue
            {
                switch (to)
                {
                case ColumnType::kBoolean:
                    UNREACHABLE();
                case ColumnType::kInteger:
                    return value;
                case ColumnType::kReal:
                    return static_cast<ColumnValueReal>(value);
                case ColumnType::kVarchar:
                    return ColumnValueToString(value, false);
                }
                UNREACHABLE();
            },
            [to](const ColumnValueReal& value) -> ColumnValue
            {
                switch (to)
                {
                case ColumnType::kBoolean:
                    UNREACHABLE();
                case ColumnType::kInteger:
                    return static_cast<ColumnValueInteger>(value);
                case ColumnType::kReal:
                    return value;
                case ColumnType::kVarchar:
                    return ColumnValueToString(value, false);
                }
                UNREACHABLE();
            },
            [to](const ColumnValueVarchar& value) -> ColumnValue
            {
                switch (to)
                {
                case ColumnType::kBoolean:
                case ColumnType::kInteger:
                case ColumnType::kReal:
                    UNREACHABLE();
                case ColumnType::kVarchar:
                    return value;
                }
                UNREACHABLE();
            },
        },
        value);
}

void ValuePrint(const Value& value)
{
    std::printf("(");
    for (std::size_t i = 0; i < value.size(); i++)
    {
        std::printf("%s", ColumnValueToString(value[i], true).c_str());
        if (i + 1 < value.size())
        {
            std::printf(", ");
        }
    }
    std::printf(")");
}

std::string ValueToList(const Value& value)
{
    std::string list = "(";
    for (std::size_t i = 0; i < value.size(); i++)
    {
        list += ColumnValueToString(value[i], true);
        if (i + 1 < value.size())
        {
            list += ", ";
        }
    }
    return list + ")";
}

bool ValueEqual(const Value& a, const Value& b)
{
    ASSERT(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); i++)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}
