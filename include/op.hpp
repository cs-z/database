#pragma once

#include "value.hpp"

enum class Function : std::uint8_t
{
    AVG,
    MAX,
    MIN,
    SUM,
    COUNT,
};

inline const char* function_to_cstr(Function function)
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

std::string               op1_to_string(Op1 op, const std::string& expr);
int                       op1_prec(Op1 op);
std::optional<ColumnType> op1_compile(const std::pair<Op1, SourceText>& op,
                                      std::optional<ColumnType>         type);
ColumnValue               op1_eval(Op1 op, const ColumnValue& value);

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

const char*               op2_cstr(Op2 op);
int                       op2_prec(Op2 op);
std::optional<ColumnType> op2_compile(const std::pair<Op2, SourceText>& op,
                                      std::optional<ColumnType>         type_l,
                                      std::optional<ColumnType>         type_r);
ColumnValue               op2_eval(const std::pair<Op2, SourceText>& op, const ColumnValue& value_l,
                                   const ColumnValue& value_r);
