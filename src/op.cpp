#include "op.hpp"
#include "common.hpp"
#include "error.hpp"
#include "type.hpp"
#include "value.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>

[[noreturn]] static void ReportOp1TypeError(SourceText op_text, std::optional<ColumnType> type)
{
    std::string type_name = type ? ColumnTypeToString(*type) : "NULL";
    throw ClientError{"operator not defined on type " + std::move(type_name), std::move(op_text)};
}

[[noreturn]] static void ReportOp2TypeError(SourceText op_text, std::optional<ColumnType> type_l,
                                            std::optional<ColumnType> type_r)
{
    std::string type_l_name = type_l ? ColumnTypeToString(*type_l) : "NULL";
    std::string type_r_name = type_r ? ColumnTypeToString(*type_r) : "NULL";
    throw ClientError{"operator not defined on types " + std::move(type_l_name) + " and " +
                          std::move(type_r_name),
                      std::move(op_text)};
}

std::string Op1ToString(Op1 op, const std::string& expr)
{
    switch (op)
    {
    case Op1::kPos:
        return "+" + ("(" + expr + ")");
    case Op1::kNeg:
        return "-" + ("(" + expr + ")");
    case Op1::kIsNull:
        return ("(" + expr + ")") + " IS NULL";
    case Op1::kIsNotNull:
        return ("(" + expr + ")") + " IS NOT NULL";
    case Op1::kNot:
        return "NOT " + ("(" + expr + ")");
    }
    UNREACHABLE();
}

int Op1Prec(Op1 op)
{
    switch (op)
    {
    case Op1::kPos:
    case Op1::kNeg:
        return 7;
    case Op1::kIsNull:
    case Op1::kIsNotNull:
        return 3;
    case Op1::kNot:
        return 2;
    }
    UNREACHABLE();
}

int Op2Prec(Op2 op)
{
    switch (op)
    {
    case Op2::kArithMul:
    case Op2::kArithDiv:
    case Op2::kArithMod:
        return 6;
    case Op2::kArithAdd:
    case Op2::kArithSub:
        return 5;
    case Op2::kCompL:
    case Op2::kCompLe:
    case Op2::kCompG:
    case Op2::kCompGe:
    case Op2::kCompEq:
    case Op2::kCompNe:
        return 4;
    case Op2::kLogicAnd:
        return 1;
    case Op2::kLogicOr:
        return 0;
    }
    UNREACHABLE();
}

std::optional<ColumnType> Op1Compile(const std::pair<Op1, SourceText>& op,
                                     std::optional<ColumnType>         type)
{
    switch (op.first)
    {
    case Op1::kPos:
    case Op1::kNeg:
        if (type && !ColumnTypeIsArithmetic(*type))
        {
            ReportOp1TypeError(op.second, type);
        }
        return type;
    case Op1::kIsNull:
    case Op1::kIsNotNull:
        return ColumnType::kBoolean;
    case Op1::kNot:
        if (type != ColumnType::kBoolean)
        {
            ReportOp1TypeError(op.second, type);
        }
        return ColumnType::kBoolean;
    }
    UNREACHABLE();
}

ColumnValue Op1Eval(Op1 op, const ColumnValue& value)
{
    switch (op)
    {
    case Op1::kPos:
    {
        return std::visit(
            Overload{
                [](const ColumnValueNull&) -> ColumnValue { return ColumnValueNull{}; },
                [](const ColumnValueBoolean&) -> ColumnValue { UNREACHABLE(); },
                [](const ColumnValueInteger& value) -> ColumnValue { return +value; },
                [](const ColumnValueReal& value) -> ColumnValue { return +value; },
                [](const ColumnValueVarchar&) -> ColumnValue { UNREACHABLE(); },
            },
            value);
    }
    case Op1::kNeg:
    {
        return std::visit(
            Overload{
                [](const ColumnValueNull&) -> ColumnValue { return ColumnValueNull{}; },
                [](const ColumnValueBoolean&) -> ColumnValue { UNREACHABLE(); },
                [](const ColumnValueInteger& value) -> ColumnValue { return -value; },
                [](const ColumnValueReal& value) -> ColumnValue { return -value; },
                [](const ColumnValueVarchar&) -> ColumnValue { UNREACHABLE(); },
            },
            value);
    }
    case Op1::kIsNull:
    {
        return value.index() == 0 ? Bool::kTrue : Bool::kFalse;
    }
    case Op1::kIsNotNull:
    {
        return value.index() == 0 ? Bool::kFalse : Bool::kTrue;
    }
    case Op1::kNot:
    {
        switch (std::get<ColumnValueBoolean>(value))
        {
        case Bool::kTrue:
            return Bool::kFalse;
        case Bool::kFalse:
            return Bool::kTrue;
        case Bool::kUnknown:
            return Bool::kUnknown;
        }
    }
    }
    UNREACHABLE();
}

const char* Op2Cstr(Op2 op)
{
    switch (op)
    {
    case Op2::kArithMul:
        return "*";
    case Op2::kArithDiv:
        return "/";
    case Op2::kArithMod:
        return "%";
    case Op2::kArithAdd:
        return "+";
    case Op2::kArithSub:
        return "-";
    case Op2::kCompL:
        return "<";
    case Op2::kCompLe:
        return "<=";
    case Op2::kCompG:
        return ">";
    case Op2::kCompGe:
        return ">=";
    case Op2::kCompEq:
        return "=";
    case Op2::kCompNe:
        return "<>";
    case Op2::kLogicAnd:
        return "AND";
    case Op2::kLogicOr:
        return "OR";
    }
    UNREACHABLE();
}

std::optional<ColumnType> Op2Compile(const std::pair<Op2, SourceText>& op,
                                     std::optional<ColumnType>         type_l,
                                     std::optional<ColumnType>         type_r)
{
    switch (op.first)
    {
    case Op2::kArithMul:
    case Op2::kArithDiv:
    case Op2::kArithMod:
    case Op2::kArithSub:
    {
        if (type_l && !ColumnTypeIsArithmetic(*type_l))
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        if (type_r && !ColumnTypeIsArithmetic(*type_r))
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        if (!type_l || !type_r)
        {
            return std::nullopt;
        }
        ASSERT(*type_l == *type_r);
        return type_l;
    }
    case Op2::kArithAdd:
    {
        if (type_l && !ColumnTypeIsArithmetic(*type_l) && *type_l != ColumnType::kVarchar)
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        if (type_r && !ColumnTypeIsArithmetic(*type_r) && *type_r != ColumnType::kVarchar)
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        if (!type_l || !type_r)
        {
            return std::nullopt;
        }
        ASSERT(*type_l == *type_r);
        return type_l;
    }
    case Op2::kCompL:
    case Op2::kCompLe:
    case Op2::kCompG:
    case Op2::kCompGe:
    {
        if (type_l && !ColumnTypeIsComparable(*type_l))
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        if (type_r && !ColumnTypeIsComparable(*type_r))
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        ASSERT(!type_l || !type_r || *type_l == *type_r);
        return ColumnType::kBoolean;
    }
    case Op2::kCompEq:
    case Op2::kCompNe:
    {
        ASSERT(!type_l || !type_r || *type_l == *type_r);
        return ColumnType::kBoolean;
    }
    case Op2::kLogicAnd:
    case Op2::kLogicOr:
    {
        if (type_l != ColumnType::kBoolean)
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        if (type_r != ColumnType::kBoolean)
        {
            ReportOp2TypeError(op.second, type_l, type_r);
        }
        return ColumnType::kBoolean;
    }
    }
    UNREACHABLE();
}

ColumnValue Op2Eval(const std::pair<Op2, SourceText>& op, const ColumnValue& value_l,
                    const ColumnValue& value_r)
{
    ASSERT(value_l.index() == 0 || value_r.index() == 0 || value_l.index() == value_r.index());
    switch (op.first)
    {
    case Op2::kArithMul:
    case Op2::kArithDiv:
    case Op2::kArithMod:
    case Op2::kArithAdd:
    case Op2::kArithSub:
    {
        if (value_l.index() == 0 || value_r.index() == 0)
        {
            return ColumnValueNull{};
        }
        return std::visit(
            Overload{
                [](const ColumnValueNull&) -> ColumnValue { UNREACHABLE(); },
                [](const ColumnValueBoolean&) -> ColumnValue { UNREACHABLE(); },
                [op, &value_r](const ColumnValueInteger& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueInteger>(value_r);
                    switch (op.first)
                    {
                    case Op2::kArithMul:
                        return ColumnValueInteger{a * b};
                    case Op2::kArithDiv:
                        if (b == 0)
                        {
                            throw ClientError{"division by zero", op.second};
                        }
                        return ColumnValueInteger{a / b};
                    case Op2::kArithMod:
                        if (b == 0)
                        {
                            throw ClientError{"division by zero", op.second};
                        }
                        return ColumnValueInteger{a % b};
                    case Op2::kArithAdd:
                        return ColumnValueInteger{a + b};
                    case Op2::kArithSub:
                        return ColumnValueInteger{a - b};
                    default:
                        UNREACHABLE();
                    }
                },
                [op, &value_r](const ColumnValueReal& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueReal>(value_r);
                    switch (op.first)
                    {
                    case Op2::kArithMul:
                        return ColumnValueReal{a * b};
                    case Op2::kArithDiv:
                        if (b == 0)
                        {
                            throw ClientError{"division by zero", op.second};
                        }
                        return ColumnValueReal{a / b};
                    case Op2::kArithAdd:
                        return ColumnValueReal{a + b};
                    case Op2::kArithSub:
                        return ColumnValueReal{a - b};
                    default:
                        UNREACHABLE();
                    }
                },
                [op, &value_r](const ColumnValueVarchar& value) -> ColumnValue
                {
                    const auto& a = value;
                    const auto& b = std::get<ColumnValueVarchar>(value_r);
                    ASSERT(op.first == Op2::kArithAdd);
                    return ColumnValueVarchar{a + b};
                },
            },
            value_l);
    }
    case Op2::kCompL:
    case Op2::kCompLe:
    case Op2::kCompG:
    case Op2::kCompGe:
    {
        if (value_l.index() == 0 || value_r.index() == 0)
        {
            return Bool::kUnknown;
        }
        return std::visit(
            Overload{
                [](const ColumnValueNull&) -> ColumnValue { UNREACHABLE(); },
                [](const ColumnValueBoolean&) -> ColumnValue { UNREACHABLE(); },
                [op, &value_r](const ColumnValueInteger& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueInteger>(value_r);
                    switch (op.first)
                    {
                    case Op2::kCompL:
                        return a < b ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompLe:
                        return a <= b ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompG:
                        return a > b ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompGe:
                        return a >= b ? Bool::kTrue : Bool::kFalse;
                    default:
                        UNREACHABLE();
                    }
                },
                [op, &value_r](const ColumnValueReal& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueReal>(value_r);
                    switch (op.first)
                    {
                    case Op2::kCompL:
                        return a < b ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompLe:
                        return a <= b ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompG:
                        return a > b ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompGe:
                        return a >= b ? Bool::kTrue : Bool::kFalse;
                    default:
                        UNREACHABLE();
                    }
                },
                [op, &value_r](const ColumnValueVarchar& value) -> ColumnValue
                {
                    const auto& a = value;
                    const auto& b = std::get<ColumnValueVarchar>(value_r);
                    switch (op.first)
                    {
                    case Op2::kCompL:
                        return CompareStrings(a, b) < 0 ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompLe:
                        return CompareStrings(a, b) <= 0 ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompG:
                        return CompareStrings(a, b) > 0 ? Bool::kTrue : Bool::kFalse;
                    case Op2::kCompGe:
                        return CompareStrings(a, b) >= 0 ? Bool::kTrue : Bool::kFalse;
                    default:
                        UNREACHABLE();
                    }
                },
            },
            value_l);
    }
    case Op2::kCompEq:
    {
        if (value_l.index() == 0 || value_r.index() == 0)
        {
            return Bool::kUnknown;
        }
        return std::visit(
            Overload{
                [](const ColumnValueNull&) -> ColumnValue { UNREACHABLE(); },
                [&value_r](const ColumnValueBoolean& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueBoolean>(value_r);
                    if (a == Bool::kUnknown)
                    {
                        return Bool::kUnknown;
                    }
                    if (b == Bool::kUnknown)
                    {
                        return Bool::kUnknown;
                    }
                    return a == b ? Bool::kTrue : Bool::kFalse;
                },
                [&value_r](const ColumnValueInteger& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueInteger>(value_r);
                    return a == b ? Bool::kTrue : Bool::kFalse;
                },
                [&value_r](const ColumnValueReal& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueReal>(value_r);
                    return a == b ? Bool::kTrue : Bool::kFalse;
                },
                [&value_r](const ColumnValueVarchar& value) -> ColumnValue
                {
                    const auto& a = value;
                    const auto& b = std::get<ColumnValueVarchar>(value_r);
                    return CompareStrings(a, b) == 0 ? Bool::kTrue : Bool::kFalse;
                },
            },
            value_l);
    }
    case Op2::kCompNe:
    {
        if (value_l.index() == 0 || value_r.index() == 0)
        {
            return Bool::kUnknown;
        }
        return std::visit(
            Overload{
                [](const ColumnValueNull&) -> ColumnValue { UNREACHABLE(); },
                [&value_r](const ColumnValueBoolean& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueBoolean>(value_r);
                    if (a == Bool::kUnknown)
                    {
                        return Bool::kUnknown;
                    }
                    if (b == Bool::kUnknown)
                    {
                        return Bool::kUnknown;
                    }
                    return a != b ? Bool::kTrue : Bool::kFalse;
                },
                [&value_r](const ColumnValueInteger& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueInteger>(value_r);
                    return a != b ? Bool::kTrue : Bool::kFalse;
                },
                [&value_r](const ColumnValueReal& value) -> ColumnValue
                {
                    const auto a = value;
                    const auto b = std::get<ColumnValueReal>(value_r);
                    return a != b ? Bool::kTrue : Bool::kFalse;
                },
                [&value_r](const ColumnValueVarchar& value) -> ColumnValue
                {
                    const auto& a = value;
                    const auto& b = std::get<ColumnValueVarchar>(value_r);
                    return CompareStrings(a, b) != 0 ? Bool::kTrue : Bool::kFalse;
                },
            },
            value_l);
    }
    case Op2::kLogicAnd:
    {
        const auto a = std::get<ColumnValueBoolean>(value_l);
        const auto b = std::get<ColumnValueBoolean>(value_r);
        switch (a)
        {
        case Bool::kTrue:
            switch (b)
            {
            case Bool::kTrue:
                return Bool::kTrue;
            case Bool::kFalse:
                return Bool::kFalse;
            case Bool::kUnknown:
                return Bool::kUnknown;
            }
            UNREACHABLE();
        case Bool::kFalse:
            return Bool::kFalse;
        case Bool::kUnknown:
            return Bool::kUnknown;
        }
        UNREACHABLE();
    }
    case Op2::kLogicOr:
    {
        const auto a = std::get<ColumnValueBoolean>(value_l);
        const auto b = std::get<ColumnValueBoolean>(value_r);
        switch (a)
        {
        case Bool::kTrue:
            return Bool::kTrue;
        case Bool::kFalse:
            switch (b)
            {
            case Bool::kTrue:
                return Bool::kTrue;
            case Bool::kFalse:
                return Bool::kFalse;
            case Bool::kUnknown:
                return Bool::kUnknown;
            }
            UNREACHABLE();
        case Bool::kUnknown:
            switch (b)
            {
            case Bool::kTrue:
                return Bool::kTrue;
            case Bool::kFalse:
            case Bool::kUnknown:
                return Bool::kUnknown;
            }
            UNREACHABLE();
        }
        UNREACHABLE();
    }
    }
    UNREACHABLE();
}
