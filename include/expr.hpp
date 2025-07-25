#pragma once

#include "common.hpp"
#include "value.hpp"
#include "op.hpp"
#include "row.hpp"

struct Expr;
using ExprPtr = std::unique_ptr<Expr>;

struct Expr
{
	struct DataConstant
	{
		ColumnValue value;
	};
	struct DataColumn
	{
		row::ColumnId column_id;
	};
	struct DataCast
	{
		ExprPtr expr;
		ColumnType to;
	};
	struct DataOp1
	{
		ExprPtr expr;
		std::pair<Op1, Text> op;
	};
	struct DataOp2
	{
		ExprPtr expr_l, expr_r;
		std::pair<Op2, Text> op;
	};
	struct DataBetween
	{
		ExprPtr expr, min, max;
		bool negated;
		Text between_text;
	};
	struct DataIn
	{
		ExprPtr expr;
		std::vector<ExprPtr> list;
		bool negated;
	};
	struct DataFunction
	{
		row::ColumnId column_id;
	};

	using Data = std::variant<DataConstant, DataColumn, DataCast, DataOp1, DataOp2,DataBetween, DataIn, DataFunction>;

	Data data;
	std::optional<ColumnType> type;

	void print() const;
	ColumnValue eval(const Value &value) const;
};
