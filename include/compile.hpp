#pragma once

#include <unordered_set>

#include "common.hpp"
#include "ast.hpp"
#include "expr.hpp"
#include "row.hpp"
#include "catalog.hpp"

struct Source;
using SourcePtr = std::unique_ptr<Source>;

struct Source
{
	struct DataTable
	{
		catalog::TableId table_id;
	};
	struct DataJoinCross
	{
		SourcePtr source_l, source_r;
	};
	struct DataJoinConditional
	{
		SourcePtr source_l, source_r;
		AstSource::DataJoinConditional::Join join; // TODO: move definition
		ExprPtr condition;
	};

	using Data = std::variant<DataTable, DataJoinCross, DataJoinConditional>;

	Data data;
	row::Type type;

	void print() const;
};

struct Aggregates
{
	struct Aggregate
	{
		Function function;
		ExprPtr arg;
	};
	using GroupBy = std::vector<row::ColumnId>;
	std::vector<Aggregate> exprs;
	GroupBy group_by;
};

struct SelectList
{
	std::vector<ExprPtr> exprs; // may containt hidden columns for sorting
	row::Type type;
	row::ColumnId visible_count;
};

struct Select
{
	SourcePtr source;
	ExprPtr where;
	Aggregates aggregates;
	SelectList list;
};

struct OrderBy
{
	struct Column
	{
		row::ColumnId column;
		bool asc;
	};
	std::vector<Column> columns;
};

struct Query
{
	Select select;
	std::optional<OrderBy> order_by;
	catalog::TableDef table_def;
};

struct InsertValue
{
	catalog::TableId table_id;
	Value value;
};

Query compile_query(const AstQuery &ast);
InsertValue compile_insert_value(AstInsertValue &ast);
