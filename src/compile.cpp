#include <algorithm>

#include "aggregate.hpp"
#include "catalog.hpp"
#include "compile.hpp"
#include "expr.hpp"
#include "sort.hpp"

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
        SourcePtr                            source_l, source_r;
        AstSource::DataJoinConditional::Join join;  // TODO: move definition
        ExprPtr                              condition;
    };

    using Data = std::variant<DataTable, DataJoinCross, DataJoinConditional>;

    Data data;
    Type type;

    Source(Data data, Type type) : data{std::move(data)}, type{std::move(type)} {}

    void print() const;
};

struct SelectList
{
    std::vector<ExprPtr> exprs;  // may containt hidden columns for sorting
    Type                 type;
    ColumnId             visible_count;
};

struct Select
{
    SourcePtr  source;
    ExprPtr    where;
    Aggregates aggregates;
    ExprPtr    having;
    SelectList list;
};

struct QueryTodo
{
    Select                 select;
    std::optional<OrderBy> order_by;
};

class Columns
{
  public:
    Columns() = default;

    Columns(SourceText&& table, catalog::NamedColumns table_columns)
    {
        for (auto& table_column : table_columns)
        {
            columns.push_back(std::move(table_column));
            columns_table.push_back(table.get());
        }
        tables.push_back({std::move(table), {ColumnId{}, ColumnId(table_columns.size())}});
    }

    Columns(const Columns& columns_l, const Columns& columns_r)
    {
        tables                = columns_l.tables;
        const ColumnId offset = tables.back().columns.second;
        for (const Table& table : columns_r.tables)
        {
            if (find_table(table.name.get()))
            {
                throw ClientError{"table name or alias collision", table.name};
            }
            const std::pair columns{table.columns.first + offset, table.columns.second + offset};
            tables.push_back({table.name, columns});
        }
        columns = columns_l.columns;
        for (const auto& column : columns_r.columns)
        {
            columns.push_back(column);
        }
        columns_table = columns_l.columns_table;
        for (const std::string& table : columns_r.columns_table)
        {
            columns_table.push_back(table);
        }
    }

    [[nodiscard]] std::pair<ColumnId, ColumnId> get_table(const SourceText& name) const
    {
        const std::optional<ColumnId> id = find_table(name.get());
        if (!id)
        {
            throw ClientError{"table not found", name};
        }
        return tables[id->get()].columns;
    }

    [[nodiscard]] std::pair<ColumnId, ColumnType>
    get_column(const AstExpr::DataColumn& column) const
    {
        std::vector<ColumnId> ids;
        if (column.table)
        {
            const auto [begin, end] = get_table(*column.table);
            for (ColumnId column_id{begin}; column_id < end; column_id++)
            {
                if (columns[column_id.get()].first == column.name.get())
                {
                    ids.push_back(column_id);
                }
            }
        }
        else
        {
            for (ColumnId column_id{}; column_id < columns.size(); column_id++)
            {
                if (columns[column_id.get()].first == column.name.get())
                {
                    ids.push_back(column_id);
                }
            }
        }
        if (ids.empty())
        {
            throw ClientError{"column not found", column.name};
        }
        if (ids.size() > 1)
        {
            throw ClientError{"column name is ambiguous", column.name};
        }
        const ColumnId id = ids.front();
        return {id, columns[id.get()].second};
    }

    [[nodiscard]] catalog::NamedColumns get_table_columns() const
    {
        std::unordered_map<std::string, int> multiplicity;
        for (const auto& [name, type] : columns)
        {
            multiplicity[name]++;
        }
        catalog::NamedColumns table_columns;
        for (std::size_t column_id = 0; column_id < columns.size(); column_id++)
        {
            const auto& [column_name, column_type] = columns[column_id];
            std::string prefix =
                multiplicity[column_name] > 1 ? (columns_table[column_id] + ".") : "";
            std::string name = std::move(prefix) + column_name;
            table_columns.emplace_back(std::move(name), column_type);
        }
        return table_columns;
    }

    [[nodiscard]] Type get_type() const
    {
        Type type;
        for (const auto& [column_name, column_type] : columns)
        {
            type.push(column_type);
        }
        return type;
    }

  private:
    [[nodiscard]] std::optional<ColumnId> find_table(const std::string& name) const
    {
        for (ColumnId column_id{}; column_id < tables.size(); column_id++)
        {
            if (tables[column_id.get()].name.get() == name)
            {
                return column_id;
            }
        }
        return std::nullopt;
    }

    struct Table
    {
        SourceText                    name;
        std::pair<ColumnId, ColumnId> columns;
    };

    std::vector<Table>                tables;
    std::vector<catalog::NamedColumn> columns;
    std::vector<std::string>          columns_table;
};

static std::optional<ColumnType> cast_together(ColumnType type_l, ColumnType type_r)
{
    if (type_l == type_r)
    {
        return type_l;
    }
    if (type_l == ColumnType::INTEGER && type_r == ColumnType::REAL)
    {
        return ColumnType::REAL;
    }
    if (type_l == ColumnType::REAL && type_r == ColumnType::INTEGER)
    {
        return ColumnType::REAL;
    }
    return std::nullopt;
}

static std::optional<ColumnType> cast_together(const std::vector<std::optional<ColumnType>>& list,
                                               const SourceText&                             text)
{
    std::optional<ColumnType> type;
    for (std::optional<ColumnType> element : list)
    {
        if (!element)
        {
            continue;
        }
        if (!type)
        {
            type = element;
            continue;
        }
        type = cast_together(*type, *element);
        if (!type)
        {
            throw ClientError{"incompatible operand types", text};
        }
    }
    return type;
}

struct ExprContext
{
    std::unordered_map<ColumnId, SourceText>& nonaggregated_columns;
    Aggregates&                               aggregates;
    const bool                                inside_aggregation;
};

static ExprPtr create_nonaggregated_column_expr(ColumnId column_id, ColumnType column_type,
                                                const SourceText& column_text, ExprContext context)
{
    if (!context.nonaggregated_columns.contains(column_id))
    {
        context.nonaggregated_columns.insert({column_id, column_text});
    }
    if (!context.aggregates.group_by.empty())
    {
        ColumnId key_id{};
        while (key_id < context.aggregates.group_by.size() &&
               context.aggregates.group_by.at(key_id.get()) != column_id)
        {
            key_id++;
        }
        if (key_id == context.aggregates.group_by.size())
        {
            throw ClientError{"nonaggregated column is not in group by clause", column_text};
        }
        return std::make_unique<Expr>(Expr::DataColumn{key_id}, column_type);
    }
    return std::make_unique<Expr>(Expr::DataColumn{column_id}, column_type);
}

static ExprPtr compile_expr(const AstExpr& ast, const Columns& columns,
                            std::optional<ExprContext> context)
{
    const SourceText text = ast.text;
    return std::visit(
        Overload{
            [](const AstExpr::DataConstant& ast)
            {
                const std::optional<ColumnType> type = column_value_to_type(ast.value);
                return std::make_unique<Expr>(Expr::DataConstant{ast.value}, type);
            },
            [&columns, context, text](const AstExpr::DataColumn& ast)
            {
                const auto [column_id, column_type] = columns.get_column(ast);
                if (context && !context->inside_aggregation)
                {
                    return create_nonaggregated_column_expr(column_id, column_type, text, *context);
                }
                return std::make_unique<Expr>(Expr::DataColumn{column_id}, column_type);
            },
            [&columns, context](const AstExpr::DataCast& ast)
            {
                ExprPtr expr = compile_expr(*ast.expr, columns, context);
                compile_cast(expr->type, ast.to);
                return std::make_unique<Expr>(Expr::DataCast{std::move(expr), ast.to.first},
                                              ast.to.first);
            },
            [&columns, context](const AstExpr::DataOp1& ast)
            {
                ExprPtr                         expr = compile_expr(*ast.expr, columns, context);
                const std::optional<ColumnType> type = op1_compile(ast.op, expr->type);
                return std::make_unique<Expr>(Expr::DataOp1{std::move(expr), ast.op}, type);
            },
            [&columns, context](const AstExpr::DataOp2& ast)
            {
                ExprPtr expr_l = compile_expr(*ast.expr_l, columns, context);
                ExprPtr expr_r = compile_expr(*ast.expr_r, columns, context);
                const std::optional<ColumnType> type =
                    cast_together({expr_l->type, expr_r->type}, ast.op.second);
                if (expr_l->type && type && *expr_l->type != *type)
                {
                    expr_l =
                        std::make_unique<Expr>(Expr::DataCast{std::move(expr_l), *type}, *type);
                }
                if (expr_r->type && type && *expr_r->type != *type)
                {
                    expr_r =
                        std::make_unique<Expr>(Expr::DataCast{std::move(expr_r), *type}, *type);
                }
                const std::optional<ColumnType> output_type =
                    op2_compile(ast.op, expr_l->type, expr_r->type);
                return std::make_unique<Expr>(
                    Expr::DataOp2{std::move(expr_l), std::move(expr_r), ast.op}, output_type);
            },
            [&columns, context](const AstExpr::DataBetween& ast)
            {
                ExprPtr                         expr = compile_expr(*ast.expr, columns, context);
                ExprPtr                         min  = compile_expr(*ast.min, columns, context);
                ExprPtr                         max  = compile_expr(*ast.max, columns, context);
                const std::optional<ColumnType> type =
                    cast_together({expr->type, min->type, max->type}, ast.between_text);
                if (type && !column_type_is_comparable(*type))
                {
                    throw ClientError{"incompatible operand types", ast.between_text};
                }
                if (expr->type && type && *expr->type != *type)
                {
                    expr = std::make_unique<Expr>(Expr::DataCast{std::move(expr), *type}, *type);
                }
                if (min->type && type && *min->type != *type)
                {
                    min = std::make_unique<Expr>(Expr::DataCast{std::move(min), *type}, *type);
                }
                if (max->type && type && *max->type != *type)
                {
                    max = std::make_unique<Expr>(Expr::DataCast{std::move(max), *type}, *type);
                }
                return std::make_unique<Expr>(Expr::DataBetween{std::move(expr), std::move(min),
                                                                std::move(max), ast.negated,
                                                                ast.between_text},
                                              ColumnType::BOOLEAN);
            },
            [&columns, context](const AstExpr::DataIn& ast)
            {
                ExprPtr              expr = compile_expr(*ast.expr, columns, context);
                std::vector<ExprPtr> list;
                std::vector<std::optional<ColumnType>> types;
                for (const AstExprPtr& ast_element : ast.list)
                {
                    ExprPtr element = compile_expr(*ast_element, columns, context);
                    types.push_back(element->type);
                    list.push_back(std::move(element));
                }
                const std::optional<ColumnType> type = cast_together(types, ast.in_text);
                if (expr->type && type && *expr->type != *type)
                {
                    expr = std::make_unique<Expr>(Expr::DataCast{std::move(expr), *type}, type);
                }
                for (ExprPtr& element : list)
                {
                    if (element->type && type && *element->type != *type)
                    {
                        element =
                            std::make_unique<Expr>(Expr::DataCast{std::move(element), *type}, type);
                    }
                }
                return std::make_unique<Expr>(
                    Expr::DataIn{std::move(expr), std::move(list), ast.negated},
                    ColumnType::BOOLEAN);
            },
            [&columns, context](const AstExpr::DataFunction& ast)
            {
                ASSERT(context);
                ASSERT(!context->inside_aggregation);
                ExprPtr                   arg;
                std::optional<ColumnType> type;
                if (ast.arg)
                {
                    arg = compile_expr(
                        *ast.arg, columns,
                        ExprContext{context->nonaggregated_columns, context->aggregates, true});
                    switch (ast.function)
                    {
                    case Function::AVG:
                    case Function::SUM:
                        if (arg->type && !column_type_is_arithmetic(*arg->type))
                        {
                            throw ClientError{"invalid argument type", ast.arg->text};
                        }
                        type = arg->type;
                        break;
                    case Function::MAX:
                    case Function::MIN:
                        if (arg->type && !column_type_is_comparable(*arg->type))
                        {
                            throw ClientError{"invalid argument type", ast.arg->text};
                        }
                        type = arg->type;
                        break;
                    case Function::COUNT:
                        type = ColumnType::INTEGER;
                        break;
                    }
                }
                else
                {
                    ASSERT(ast.function == Function::COUNT);
                    type = ColumnType::INTEGER;
                }
                const ColumnId column_id(context->aggregates.group_by.size() +
                                         context->aggregates.exprs.size());
                context->aggregates.exprs.push_back({ast.function, std::move(arg)});
                return std::make_unique<Expr>(Expr::DataFunction{column_id}, type);
            },
        },
        ast.data);
}

static std::pair<SourcePtr, Columns> compile_source(const AstSource& ast)
{
    return std::visit(
        Overload{
            [](const AstSource::DataTable& ast)
            {
                auto [table_id, table_columns] = catalog::get_table_named(ast.name);
                Columns columns{ast.alias.value_or(ast.name), std::move(table_columns)};
                return std::make_pair(
                    std::make_unique<Source>(Source::DataTable{table_id}, columns.get_type()),
                    std::move(columns));
            },
            [](const AstSource::DataJoinCross& ast)
            {
                auto [source_l, columns_l] = compile_source(*ast.source_l);
                auto [source_r, columns_r] = compile_source(*ast.source_r);
                Columns columns{columns_l, columns_r};
                return std::make_pair(
                    std::make_unique<Source>(
                        Source::DataJoinCross{std::move(source_l), std::move(source_r)},
                        columns.get_type()),
                    std::move(columns));
            },
            [](const AstSource::DataJoinConditional& ast)
            {
                auto [source_l, columns_l] = compile_source(*ast.source_l);
                auto [source_r, columns_r] = compile_source(*ast.source_r);
                Columns columns{columns_l, columns_r};
                ExprPtr condition = compile_expr(*ast.condition, columns, std::nullopt);
                if (condition->type != ColumnType::BOOLEAN)
                {
                    throw ClientError{"condition must be boolean", ast.condition->text};
                }
                return std::make_pair(
                    std::make_unique<Source>(
                        Source::DataJoinConditional{
                            std::move(source_l), std::move(source_r),
                            ast.join.value_or(AstSource::DataJoinConditional::Join::INNER),
                            std::move(condition)},
                        columns.get_type()),
                    std::move(columns));
            },
        },
        ast.data);
};

static std::pair<SourcePtr, Columns> compile_sources(const std::vector<AstSourcePtr>& asts)
{
    ASSERT(!asts.empty());
    std::pair<SourcePtr, Columns> result = compile_source(*asts[0]);
    for (std::size_t i = 1; i < asts.size(); i++)
    {
        std::pair<SourcePtr, Columns> other = compile_source(*asts[i]);
        Columns                       columns{result.second, other.second};
        result =
            std::make_pair(std::make_unique<Source>(Source::DataJoinCross{std::move(result.first),
                                                                          std::move(other.first)},
                                                    columns.get_type()),
                           std::move(columns));
    }
    return result;
};

static ExprPtr compile_where(const Columns& columns, const AstExprPtr& ast)
{
    if (!ast)
    {
        return ExprPtr{};
    }
    ExprPtr expr = compile_expr(*ast, columns, std::nullopt);
    if (expr->type != ColumnType::BOOLEAN)
    {
        throw ClientError{"condition must be boolean", ast->text};
    }
    return expr;
}

static Aggregates::GroupBy compile_group_by(const Columns&                   columns,
                                            const std::optional<AstGroupBy>& ast)
{
    Aggregates::GroupBy group_by;
    if (ast)
    {
        for (const auto& [ast_column, text] : ast->columns)
        {
            const ColumnId column_id = columns.get_column(ast_column).first;
            group_by.push_back(column_id);
        }
    }
    return group_by;
}

static ExprPtr compile_having(const Columns& columns, const AstExprPtr& ast, ExprContext context)
{
    if (!ast)
    {
        return ExprPtr{};
    }
    ExprPtr expr = compile_expr(*ast, columns, context);
    if (expr->type != ColumnType::BOOLEAN)
    {
        throw ClientError{"condition must be boolean", ast->text};
    }
    return expr;
}

static std::pair<SelectList, catalog::NamedColumns>
compile_select_list(const Columns& columns, const AstSelectList& ast,
                    std::unordered_map<ColumnId, SourceText>& nonaggregated_columns,
                    Aggregates&                               aggregates)
{
    const catalog::NamedColumns column_names = columns.get_table_columns();
    SelectList                  list         = {};
    catalog::NamedColumns       table_columns;
    for (const AstSelectList::Element& ast_element : ast.elements)
    {
        // TODO: capture list too long
        std::visit(
            Overload{
                [&nonaggregated_columns, &aggregates, &column_names, &list,
                 &table_columns](const AstSelectList::Wildcard& ast_element)
                {
                    for (ColumnId column_id{}; column_id < column_names.size(); column_id++)
                    {
                        const auto& [column_name, column_type] = column_names[column_id.get()];
                        ExprPtr expr                           = create_nonaggregated_column_expr(
                            column_id, column_type, ast_element.asterisk_text,
                            ExprContext{nonaggregated_columns, aggregates, false});
                        list.exprs.push_back(std::move(expr));
                        list.type.push(column_type);
                        list.visible_count++;
                        table_columns.emplace_back(column_name, column_type);
                    }
                },
                [&nonaggregated_columns, &aggregates, &columns, &column_names, &list,
                 &table_columns](const AstSelectList::TableWildcard& ast_element)
                {
                    const auto [begin, end] = columns.get_table(ast_element.table);
                    for (ColumnId column_id{begin}; column_id < end; column_id++)
                    {
                        const auto& [column_name, column_type] = column_names[column_id.get()];
                        ExprPtr expr                           = create_nonaggregated_column_expr(
                            column_id, column_type, ast_element.asterisk_text,
                            ExprContext{nonaggregated_columns, aggregates, false});
                        list.exprs.push_back(std::move(expr));
                        list.type.push(column_type);
                        list.visible_count++;
                        table_columns.emplace_back(column_name, column_type);
                    }
                },
                [&nonaggregated_columns, &aggregates, &columns, &list,
                 &table_columns](const AstSelectList::Expr& ast_element)
                {
                    ExprPtr expr =
                        compile_expr(*ast_element.expr, columns,
                                     ExprContext{nonaggregated_columns, aggregates, false});
                    const ColumnType column_type = expr->type.value_or(ColumnType::INTEGER);
                    std::string      column_name = ast_element.alias ? ast_element.alias->get()
                                                                     : ast_element.expr->to_string();
                    const auto       iter = std::find_if(table_columns.begin(), table_columns.end(),
                                                         [&column_name](const auto& column)
                                                         { return column.first == column_name; });
                    if (iter != table_columns.end())
                    {
                        throw ClientError{"column name or alias collision",
                                          ast_element.alias.value_or(ast_element.expr->text)};
                    }
                    list.exprs.push_back(std::move(expr));
                    list.type.push(column_type);
                    list.visible_count++;
                    table_columns.emplace_back(std::move(column_name), column_type);
                },
            },
            ast_element);
    }
    return std::make_pair(std::move(list), std::move(table_columns));
}

static std::tuple<Select, Columns, catalog::NamedColumns> compile_select(const AstSelect& ast)
{
    auto [source, columns] = compile_sources(ast.sources);
    ExprPtr    where       = compile_where(columns, ast.where);
    Aggregates aggregates  = {
        std::vector<Aggregates::Aggregate>{},
        compile_group_by(columns, ast.group_by),
    };
    std::unordered_map<ColumnId, SourceText> nonaggregated_columns;
    auto [list, table_columns] =
        compile_select_list(columns, ast.list, nonaggregated_columns, aggregates);
    ExprPtr having =
        compile_having(columns, ast.having, ExprContext{nonaggregated_columns, aggregates, false});
    if (!aggregates.exprs.empty() && aggregates.group_by.empty() && !nonaggregated_columns.empty())
    {
        throw ClientError{"nonaggregated column in aggregation",
                          nonaggregated_columns.begin()->second};
    }
    return std::make_tuple(Select{std::move(source), std::move(where), std::move(aggregates),
                                  std::move(having), std::move(list)},
                           std::move(columns), std::move(table_columns));
}

static OrderBy compile_order_by(const Columns& columns, SelectList& list, const AstOrderBy& ast)
{
    OrderBy order_by;
    for (const AstOrderBy::Column& ast_column : ast.columns)
    {
        const ColumnId column = std::visit(
            Overload{
                [&list](const AstOrderBy::Index& column)
                {
                    if (column.index.first <= 0 || column.index.first > list.exprs.size())
                    {
                        throw ClientError{"invalid column index", column.index.second};
                    }
                    return column.index.first - 1;
                },
                [&columns, &list](const AstExpr::DataColumn& column)
                {
                    const auto [column_id, column_type] = columns.get_column(column);
                    ColumnId expr_id{};
                    for (; expr_id < list.exprs.size(); expr_id++)
                    {
                        if (const Expr::DataColumn* expr =
                                std::get_if<Expr::DataColumn>(&list.exprs[expr_id.get()]->data))
                        {
                            if (expr->column_id == column_id)
                            {
                                break;
                            }
                        }
                    }
                    if (expr_id < list.exprs.size())
                    {
                        return expr_id;
                    }
                    const ColumnId extra_id(list.exprs.size());
                    list.exprs.push_back(
                        std::make_unique<Expr>(Expr::DataColumn{column_id}, column_type));
                    list.type.push(column_type);
                    return extra_id;
                },
            },
            ast_column.column);
        order_by.columns.push_back({column, ast_column.asc});
    }
    return order_by;
}

static Iter create_source_iter(Source& source)
{
    Type& type = source.type;
    return std::visit(
        Overload{
            [&type](Source::DataTable& source) -> Iter
            {
                return std::make_unique<IterScan>(catalog::get_table_file_ids(source.table_id),
                                                  std::move(type));
            },
            [&type](Source::DataJoinCross& source) -> Iter
            {
                Iter iter_l = create_source_iter(*source.source_l);
                Iter iter_r = create_source_iter(*source.source_r);
                return std::make_unique<IterJoinCross>(std::move(iter_l), std::move(iter_r),
                                                       std::move(type));
            },
            [&type](Source::DataJoinConditional& source) -> Iter
            {
                Iter iter_l = create_source_iter(*source.source_l);
                Iter iter_r = create_source_iter(*source.source_r);
                return std::make_unique<IterJoinQualified>(std::move(iter_l), std::move(iter_r),
                                                           std::move(source.condition),
                                                           std::move(type));
            },
        },
        source.data);
}

static Iter create_select_iter(Select& select)
{
    Iter source = create_source_iter(*select.source);
    if (select.where)
    {
        source = std::make_unique<IterFilter>(std::move(source), std::move(select.where));
    }
    if (!select.aggregates.group_by.empty() || !select.aggregates.exprs.empty())
    {
        source = std::make_unique<IterAggregate>(std::move(source), std::move(select.aggregates));
    }
    if (select.having)
    {
        source = std::make_unique<IterFilter>(std::move(source), std::move(select.having));
    }
    return std::make_unique<IterExpr>(std::move(source), std::move(select.list.exprs),
                                      std::move(select.list.type));
}

static Iter create_query_iter(QueryTodo&& query)
{
    Iter iter = create_select_iter(query.select);
    if (query.order_by)
    {
        iter = std::make_unique<IterSort>(std::move(iter), std::move(*query.order_by));
        std::vector<ColumnId> columns;
        for (ColumnId column_id{}; column_id < query.select.list.visible_count; column_id++)
        {
            columns.push_back(column_id);
        }
        iter = std::make_unique<IterProject>(std::move(iter), std::move(columns));
    }
    return iter;
}

static Query compile_query(const AstQuery& ast)
{
    auto [select, columns, table_columns] = compile_select(ast.select);
    std::optional<OrderBy> order_by;
    if (ast.order_by)
    {
        order_by = compile_order_by(columns, select.list, *ast.order_by);
    }
    QueryTodo query{std::move(select), std::move(order_by)};
    return {std::move(table_columns), create_query_iter(std::move(query)), ast.limit};
}

static CreateTable compile_create_table(AstCreateTable& ast)
{
    std::string name = ast.name.get();  // TODO: .get() move?
    if (catalog::find_table(name))
    {
        throw ClientError{"table already exists", std::move(ast.name)};
    }
    return {std::move(name), std::move(ast.columns)};
}

static InsertValue compile_insert_value(AstInsertValue& ast)
{
    auto [table_id, type] = catalog::get_table(ast.table);
    if (ast.exprs.size() != type.size())
    {
        throw ClientError{"column number mismatch"};
    }
    Value   value;
    Columns columns;
    for (std::size_t i = 0; i < type.size(); i++)
    {
        ExprPtr expr = compile_expr(*ast.exprs[i], columns, std::nullopt);
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        if (expr->type.has_value() && expr->type.value() != type.at(i))
        {
            throw ClientError{"column type mismatch", ast.exprs[i]->text};
        }
        value.push_back(expr->eval(nullptr));
    }
    return {table_id, std::move(type), std::move(value)};
}

Statement compile_statement(AstStatement& ast)
{
    return std::visit(
        Overload{
            [](AstCreateTable& ast) -> Statement { return compile_create_table(ast); },
            [](AstInsertValue& ast) -> Statement { return compile_insert_value(ast); },
            [](AstQuery& ast) -> Statement { return compile_query(ast); },
        },
        ast);
}
