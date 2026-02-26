#pragma once

#include <unordered_set>

#include "ast.hpp"
#include "catalog.hpp"
#include "iter.hpp"
#include "type.hpp"

struct CreateTable
{
    std::string           name;
    catalog::NamedColumns columns;
};

struct InsertValue
{
    catalog::TableId table_id;
    Type             type;  // TODO: avoid copy
    Value            value;
};

struct Query
{
    catalog::NamedColumns       columns;
    Iter                        iter;
    std::optional<unsigned int> limit;
};

using Statement = std::variant<CreateTable, InsertValue, Query>;

Statement compile_statement(AstStatement& ast);
