#pragma once

#include "error.hpp"
#include "type.hpp"
#include "value.hpp"

#include <optional>
#include <string>

enum class Function : std::uint8_t
{
    kAvg,
    kMax,
    kMin,
    kSum,
    kCount,
};

inline const char* FunctionToCstr(Function function)
{
    switch (function)
    {
    case Function::kAvg:
        return "AVG";
    case Function::kMax:
        return "MAX";
    case Function::kMin:
        return "MIN";
    case Function::kSum:
        return "SUM";
    case Function::kCount:
        return "COUNT";
    }
    UNREACHABLE();
}

enum class Op1 : std::uint8_t
{
    kPos,
    kNeg,
    kIsNull,
    kIsNotNull,
    kNot,
};

std::string               Op1ToString(Op1 op, const std::string& expr);
int                       Op1Prec(Op1 op);
std::optional<ColumnType> Op1Compile(const std::pair<Op1, SourceText>& op,
                                     std::optional<ColumnType>         type);
ColumnValue               Op1Eval(Op1 op, const ColumnValue& value);

enum class Op2 : std::uint8_t
{
    kArithMul,
    kArithDiv,
    kArithMod,
    kArithAdd,
    kArithSub,
    kCompL,
    kCompLe,
    kCompG,
    kCompGe,
    kCompEq,
    kCompNe,
    kLogicAnd,
    kLogicOr,
};

const char*               Op2Cstr(Op2 op);
int                       Op2Prec(Op2 op);
std::optional<ColumnType> Op2Compile(const std::pair<Op2, SourceText>& op,
                                     std::optional<ColumnType>         type_l,
                                     std::optional<ColumnType>         type_r);
ColumnValue               Op2Eval(const std::pair<Op2, SourceText>& op, const ColumnValue& value_l,
                                  const ColumnValue& value_r);
