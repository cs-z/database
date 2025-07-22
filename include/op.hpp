#pragma once

#include "value.hpp"

enum class Op1
{
	Pos,
	Neg,
	IsNull,
	IsNotNull,
	Not,
};

std::string op1_to_string(Op1 op, const std::string &expr);
unsigned int op1_prec(Op1 op);
std::optional<ColumnType> op1_check(const std::pair<Op1, Text> &op, std::optional<ColumnType> type);
ColumnValue op1_eval(Op1 op, const ColumnValue &value);

enum class Op2
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

const char *op2_cstr(Op2 op);
unsigned int op2_prec(Op2 op);
std::optional<ColumnType> op2_check(const std::pair<Op2, Text> &op, std::optional<ColumnType> type_l, std::optional<ColumnType> type_r);
ColumnValue op2_eval(const std::pair<Op2, Text> &op, const ColumnValue &value_l, const ColumnValue &value_r);
