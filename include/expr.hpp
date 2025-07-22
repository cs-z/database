#pragma once

#include "common.hpp"
#include "value.hpp"
#include "op.hpp"

struct Expr
{
	struct DataConstant
	{
		ColumnValue value;
	};
	struct DataColumn
	{
		unsigned int id;
	};
	struct DataCast
	{
		std::unique_ptr<Expr> expr;
		ColumnType to;
	};
	struct DataOp1
	{
		std::unique_ptr<Expr> expr;
		std::pair<Op1, Text> op;
	};
	struct DataOp2
	{
		std::unique_ptr<Expr> expr_l, expr_r;
		std::pair<Op2, Text> op;
	};
	struct DataBetween
	{
		std::unique_ptr<Expr> expr, min, max;
		bool negated;
		Text between_text;
	};
	struct DataIn
	{
		std::unique_ptr<Expr> expr;
		std::vector<std::unique_ptr<Expr>> list;
		bool negated;
	};
	struct DataAggregate
	{
		unsigned int id;
	};

	using Data = std::variant<DataConstant, DataColumn, DataCast, DataOp1, DataOp2,DataBetween, DataIn, DataAggregate>;

	Data data;
	std::optional<ColumnType> type;

	void print() const;
	ColumnValue eval(const Value &row) const;
};
