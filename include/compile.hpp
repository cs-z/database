#pragma once

#include <unordered_set>

#include "catalog.hpp"
#include "ast.hpp"
#include "iter.hpp"

struct CreateTable
{
	std::string table_name;
	catalog::TableDef table_def;
};

struct InsertValue
{
	catalog::TableId table_id;
	Value value;
};

struct Query
{
	catalog::TableDef table_def;
	IterPtr iter;
	std::optional<unsigned int> limit;
};

using Statement = std::variant<CreateTable, InsertValue, Query>;

Statement compile_statement(AstStatement &ast);
