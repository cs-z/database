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

struct DropTable
{
    catalog::TableId table_id;
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

struct TruncateTable
{
    catalog::TableId table_id;
};

struct DeleteConditional
{
    catalog::TableId table_id;
    Iter             iter;
};

using Statement =
    std::variant<CreateTable, DropTable, InsertValue, Query, TruncateTable, DeleteConditional>;

Statement compile_statement(AstStatement& ast);
