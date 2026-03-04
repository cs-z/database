#pragma once

#include "error.hpp"
#include "op.hpp"
#include "type.hpp"
#include "value.hpp"

#include <optional>
#include <string>

enum class Function : std::uint8_t
{
    AVG,
    MAX,
    MIN,
    SUM,
    COUNT,
};

inline const char* FunctionToCstr(Function function)
{
    switch (function)
    {
    case Function::AVG:
        return "AVG";
    case Function::MAX:
        return "MAX";
    case Function::MIN:
        return "MIN";
    case Function::SUM:
        return "SUM";
    case Function::COUNT:
        return "COUNT";
    }
    UNREACHABLE();
}

enum class Op1 : std::uint8_t
{
    Pos,
    Neg,
    IsNull,
    IsNotNull,
    Not,
};

std::string               Op1ToString(Op1 op, const std::string& expr);
int                       Op1Prec(Op1 op);
std::optional<ColumnType> Op1Compile(const std::pair<Op1, SourceText>& op,
                                     std::optional<ColumnType>         type);
ColumnValue               Op1Eval(Op1 op, const ColumnValue& value);

enum class Op2 : std::uint8_t
{
    ArithMul,
    ArithDiv,
    ArithMod,
    ArithAdd,
    ArithSub,
    CompL,
    CompLe,
    CompG,
    CompGe,
    CompEq,
    CompNe,
    LogicAnd,
    LogicOr,
};

const char*               Op2Cstr(Op2 op);
int                       Op2Prec(Op2 op);
std::optional<ColumnType> Op2Compile(const std::pair<Op2, SourceText>& op,
                                     std::optional<ColumnType>         type_l,
                                     std::optional<ColumnType>         type_r);
ColumnValue               Op2Eval(const std::pair<Op2, SourceText>& op, const ColumnValue& value_l,
                                  const ColumnValue& value_r);
