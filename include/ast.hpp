#pragma once

#include "common.hpp"
#include "value.hpp"
#include "op.hpp"
#include "error.hpp"
#include "row.hpp"

using AstIdentifier = std::pair<std::string, Text>;

struct AstExpr;
using AstExprPtr = std::unique_ptr<AstExpr>;

struct AstExpr
{
	struct DataConstant
	{
		ColumnValue value;
	};
	struct DataColumn
	{
		std::optional<AstIdentifier> table;
		AstIdentifier name;
		std::string to_string() const;
	};
	struct DataCast
	{
		AstExprPtr expr;
		std::pair<ColumnType, Text> to;
	};
	struct DataOp1
	{
		AstExprPtr expr;
		std::pair<Op1, Text> op;
	};
	struct DataOp2
	{
		AstExprPtr expr_l, expr_r;
		std::pair<Op2, Text> op;
	};
	struct DataBetween
	{
		AstExprPtr expr, min, max;
		bool negated;
		Text between_text;
	};
	struct DataIn
	{
		AstExprPtr expr;
		std::vector<AstExprPtr> list;
		bool negated;
		Text in_text;
	};
	struct DataFunction
	{
		Function function;
		AstExprPtr arg;
	};

	using Data = std::variant<DataConstant, DataColumn, DataCast, DataOp1, DataOp2, DataBetween, DataIn, DataFunction>;

	Data data;
	Text text;

	std::string to_string() const;
	void print() const;
};

struct AstSelectList
{
	struct Wildcard
	{
		Text asterisk_text;
	};
	struct TableWildcard
	{
		AstIdentifier table;
		Text asterisk_text;
	};
	struct Expr
	{
		AstExprPtr expr;
		std::optional<AstIdentifier> alias;
	};
	using Element = std::variant<Wildcard, TableWildcard, Expr>;

	std::vector<Element> elements;
};

struct AstSource;
using AstSourcePtr = std::unique_ptr<AstSource>;

struct AstSource
{
	struct DataTable
	{
		AstIdentifier name;
		std::optional<AstIdentifier> alias;
	};
	struct DataJoinCross
	{
		AstSourcePtr source_l, source_r;
	};
	struct DataJoinConditional
	{
		enum class Join
		{
			INNER,
			LEFT,
			RIGHT,
			FULL,
		};
		AstSourcePtr source_l, source_r;
		std::optional<Join> join;
		AstExprPtr condition;
	};

	using Data = std::variant<DataTable, DataJoinCross, DataJoinConditional>;

	Data data;
	Text text;

	void print() const;
};

struct AstGroupBy
{
	std::vector<std::pair<AstExpr::DataColumn, Text>> columns;
};

struct AstSelect
{
	AstSelectList list;                              // SELECT clause
	std::vector<AstSourcePtr> sources; // FROM clause
	AstExprPtr where;                  // WHERE clause: null if missing
	std::optional<AstGroupBy> group_by;              // GROUP BY clause
};

struct AstOrderBy
{
	struct Index
	{
		std::pair<row::ColumnId, Text> index;
	};
	struct Column
	{
		std::variant<Index, AstExpr::DataColumn> column;
		bool asc;
	};
	std::vector<Column> columns;
};

struct AstQuery
{
	AstSelect select;
	std::optional<AstOrderBy> order_by;
	void print() const;
};

struct AstInsertValue
{
	AstIdentifier table;
	std::vector<AstExprPtr> exprs;
};
