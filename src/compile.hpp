#pragma once

#include "ast.hpp"
#include "catalog.hpp"
#include "iter.hpp"
#include "type.hpp"

#include <optional>
#include <string>
#include <unordered_set>

struct CreateTable
{
    std::string           name;
    catalog::NamedColumns columns;
};

struct InsertValue
{
    catalog::TableId table_id;
    Type             type; // TODO: avoid copy
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
