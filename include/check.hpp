#pragma once

#include <unordered_set>

#include "common.hpp"
#include "ast.hpp"
#include "expr.hpp"

class Columns
{
public:

	Columns(AstIdentifier table, const Type &type);
	Columns(Columns columns_l, Columns columns_r);

	std::pair<unsigned int, unsigned int> get_table(const AstIdentifier &name) const;
	std::pair<unsigned int, ColumnType> get_column(const AstExpr::DataColumn &column) const;

	Type get_type() const;

private:

	std::optional<unsigned int> find_table(const std::string &name) const;

	struct Table
	{
		AstIdentifier name;
		std::pair<unsigned int, unsigned int> columns;
	};

	struct Column
	{
		std::string table;
		std::string name;
		ColumnType type;
	};

	std::vector<Table> tables;
	std::vector<Column> columns;
};

struct Source
{
	struct DataTable
	{
		std::string name;
	};
	struct DataJoinCross
	{
		std::unique_ptr<Source> source_l, source_r;
	};
	struct DataJoinConditional
	{
		std::unique_ptr<Source> source_l, source_r;
		AstSource::DataJoinConditional::Join join; // TODO: move definition
		std::unique_ptr<Expr> condition;
	};

	using Data = std::variant<DataTable, DataJoinCross, DataJoinConditional>;

	Data data;
	Columns columns;

	void print() const;
};

struct SelectList
{
	std::vector<std::unique_ptr<Expr>> exprs;
	Type type;
	std::unordered_map<unsigned int, Text> nonaggregated_columns;
	Aggregates aggregates;
};

using GroupBy = std::vector<unsigned int>;

struct Select
{
	std::unique_ptr<Source> source;
	std::unique_ptr<Expr> where;
	std::optional<GroupBy> group_by;
	SelectList list;
};

struct OrderBy
{
	struct Column
	{
		unsigned int column;
		bool asc;
	};
	std::vector<Column> columns;
};

struct Query
{
	Select select;
	std::optional<OrderBy> order_by;
};

Query check_query(const AstQuery &ast_query);
