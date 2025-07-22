#pragma once

#include "common.hpp"
#include "value.hpp"
#include "op.hpp"
#include "aggregate.hpp"
#include "error.hpp"

using AstIdentifier = std::pair<std::string, Text>;

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
	};
	struct DataCast
	{
		std::unique_ptr<AstExpr> expr;
		std::pair<ColumnType, Text> to;
	};
	struct DataOp1
	{
		std::unique_ptr<AstExpr> expr;
		std::pair<Op1, Text> op;
	};
	struct DataOp2
	{
		std::unique_ptr<AstExpr> expr_l, expr_r;
		std::pair<Op2, Text> op;
	};
	struct DataBetween
	{
		std::unique_ptr<AstExpr> expr, min, max;
		bool negated;
		Text between_text;
	};
	struct DataIn
	{
		std::unique_ptr<AstExpr> expr;
		std::vector<std::unique_ptr<AstExpr>> list;
		bool negated;
	};
	struct DataAggregate
	{
		std::unique_ptr<AstExpr> arg;
		AggregateFunction function;
	};

	using Data = std::variant<DataConstant, DataColumn, DataCast, DataOp1, DataOp2, DataBetween, DataIn, DataAggregate>;

	Data data;
	Text text;

	std::string to_string() const;
	void print() const;
};

struct AstSelectList
{
	struct Wildcard
	{
		std::string table;
	};
	struct TableWildcard
	{
		AstIdentifier table;
	};
	struct Expr
	{
		std::unique_ptr<AstExpr> expr;
		std::optional<AstIdentifier> alias;
	};
	using Element = std::variant<Wildcard, TableWildcard, Expr>;

	std::vector<Element> elements;
};

struct AstSource
{
	struct DataTable
	{
		AstIdentifier name;
		std::optional<AstIdentifier> alias;
	};
	struct DataJoinCross
	{
		std::unique_ptr<AstSource> source_l, source_r;
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
		std::unique_ptr<AstSource> source_l, source_r;
		std::optional<Join> join;
		std::unique_ptr<AstExpr> condition;
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
	std::vector<std::unique_ptr<AstSource>> sources; // FROM clause
	std::unique_ptr<AstExpr> where;                  // WHERE clause: null if missing
	std::optional<AstGroupBy> group_by;              // GROUP BY clause
};

struct AstOrderBy
{
	struct Index
	{
		std::pair<unsigned int, Text> index;
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
