#include "compile.hpp"
#include "aggregate.hpp"
#include "ast.hpp"
#include "catalog.hpp"
#include "common.hpp"
#include "error.hpp"
#include "expr.hpp"
#include "iter.hpp"
#include "op.hpp"
#include "sort.hpp"
#include "type.hpp"
#include "value.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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
        AstSource::DataJoinConditional::Join join; // TODO: move definition
        ExprPtr                              condition;
    };

    using Data = std::variant<DataTable, DataJoinCross, DataJoinConditional>;

    Data data;
    Type type;

    Source(Data data, Type type) : data{std::move(data)}, type{std::move(type)}
    {
    }

    void Print() const;
};

struct SelectList
{
    std::vector<ExprPtr> exprs; // may containt hidden columns for sorting
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

    Columns(SourceText table, catalog::NamedColumns table_columns)
    {
        for (auto& table_column : table_columns)
        {
            columns_.push_back(std::move(table_column));
            columns_table_.push_back(table.Get());
        }
        tables_.push_back(
            {.name = std::move(table), .columns = {ColumnId{}, ColumnId(table_columns.size())}});
    }

    Columns(const Columns& columns_l, const Columns& columns_r)
    {
        tables_               = columns_l.tables_;
        const ColumnId offset = tables_.back().columns.second;
        for (const Table& table : columns_r.tables_)
        {
            if (FindTable(table.name.Get()))
            {
                throw ClientError{"table name or alias collision", table.name};
            }
            const std::pair columns{table.columns.first + offset, table.columns.second + offset};
            tables_.push_back({.name = table.name, .columns = columns});
        }
        columns_ = columns_l.columns_;
        for (const auto& column : columns_r.columns_)
        {
            columns_.push_back(column);
        }
        columns_table_ = columns_l.columns_table_;
        for (const std::string& table : columns_r.columns_table_)
        {
            columns_table_.push_back(table);
        }
    }

    [[nodiscard]] std::pair<ColumnId, ColumnId> GetTable(const SourceText& name) const
    {
        const std::optional<ColumnId> id = FindTable(name.Get());
        if (!id)
        {
            throw ClientError{"table not found", name};
        }
        return tables_[id->Get()].columns;
    }

    [[nodiscard]] std::pair<ColumnId, ColumnType> GetColumn(const AstExpr::DataColumn& column) const
    {
        std::vector<ColumnId> ids;
        if (column.table)
        {
            const auto [begin, end] = GetTable(*column.table);
            for (ColumnId column_id{begin}; column_id < end; column_id++)
            {
                if (columns_[column_id.Get()].first == column.name.Get())
                {
                    ids.push_back(column_id);
                }
            }
        }
        else
        {
            for (ColumnId column_id{}; column_id < columns_.size(); column_id++)
            {
                if (columns_[column_id.Get()].first == column.name.Get())
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
        return {id, columns_[id.Get()].second};
    }

    [[nodiscard]] catalog::NamedColumns GetTableColumns() const
    {
        std::unordered_map<std::string, int> multiplicity;
        for (const auto& [name, type] : columns_)
        {
            multiplicity[name]++;
        }
        catalog::NamedColumns table_columns;
        for (std::size_t column_id = 0; column_id < columns_.size(); column_id++)
        {
            const auto& [column_name, column_type] = columns_[column_id];
            std::string prefix =
                multiplicity[column_name] > 1 ? (columns_table_[column_id] + ".") : "";
            std::string name = std::move(prefix) + column_name;
            table_columns.emplace_back(std::move(name), column_type);
        }
        return table_columns;
    }

    [[nodiscard]] Type GetType() const
    {
        Type type;
        for (const auto& [column_name, column_type] : columns_)
        {
            type.Push(column_type);
        }
        return type;
    }

private:
    [[nodiscard]] std::optional<ColumnId> FindTable(const std::string& name) const
    {
        for (ColumnId column_id{}; column_id < tables_.size(); column_id++)
        {
            if (tables_[column_id.Get()].name.Get() == name)
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

    std::vector<Table>                tables_;
    std::vector<catalog::NamedColumn> columns_;
    std::vector<std::string>          columns_table_;
};

[[nodiscard]] static std::optional<ColumnType> CastTogether(ColumnType type_l, ColumnType type_r)
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

[[nodiscard]] static std::optional<ColumnType>
CastTogether(const std::vector<std::optional<ColumnType>>& list, const SourceText& text)
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
        type = CastTogether(*type, *element);
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

[[nodiscard]] static ExprPtr CreateNonaggregatedColumnExpr(ColumnId          column_id,
                                                           ColumnType        column_type,
                                                           const SourceText& column_text,
                                                           ExprContext       context)
{
    if (!context.nonaggregated_columns.contains(column_id))
    {
        context.nonaggregated_columns.insert({column_id, column_text});
    }
    if (!context.aggregates.group_by.empty())
    {
        ColumnId key_id{};
        while (key_id < context.aggregates.group_by.size() &&
               context.aggregates.group_by.at(key_id.Get()) != column_id)
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

[[nodiscard]] static ExprPtr CompileExpr(const AstExpr& ast, const Columns& columns,
                                         std::optional<ExprContext> context)
{
    const SourceText text = ast.text;
    return std::visit(
        Overload{
            [](const AstExpr::DataConstant& ast)
            {
                const std::optional<ColumnType> type = ColumnValueToType(ast.value);
                return std::make_unique<Expr>(Expr::DataConstant{ast.value}, type);
            },
            [&columns, context, text](const AstExpr::DataColumn& ast)
            {
                const auto [column_id, column_type] = columns.GetColumn(ast);
                if (context && !context->inside_aggregation)
                {
                    return CreateNonaggregatedColumnExpr(column_id, column_type, text, *context);
                }
                return std::make_unique<Expr>(Expr::DataColumn{column_id}, column_type);
            },
            [&columns, context](const AstExpr::DataCast& ast)
            {
                ExprPtr expr = CompileExpr(*ast.expr, columns, context);
                CompileCast(expr->type, ast.to);
                return std::make_unique<Expr>(
                    Expr::DataCast{.expr = std::move(expr), .to = ast.to.first}, ast.to.first);
            },
            [&columns, context](const AstExpr::DataOp1& ast)
            {
                ExprPtr                         expr = CompileExpr(*ast.expr, columns, context);
                const std::optional<ColumnType> type = Op1Compile(ast.op, expr->type);
                return std::make_unique<Expr>(Expr::DataOp1{.expr = std::move(expr), .op = ast.op},
                                              type);
            },
            [&columns, context](const AstExpr::DataOp2& ast)
            {
                ExprPtr                         expr_l = CompileExpr(*ast.expr_l, columns, context);
                ExprPtr                         expr_r = CompileExpr(*ast.expr_r, columns, context);
                const std::optional<ColumnType> type =
                    CastTogether({expr_l->type, expr_r->type}, ast.op.second);
                if (expr_l->type && type && *expr_l->type != *type)
                {
                    expr_l = std::make_unique<Expr>(
                        Expr::DataCast{.expr = std::move(expr_l), .to = *type}, *type);
                }
                if (expr_r->type && type && *expr_r->type != *type)
                {
                    expr_r = std::make_unique<Expr>(
                        Expr::DataCast{.expr = std::move(expr_r), .to = *type}, *type);
                }
                const std::optional<ColumnType> output_type =
                    Op2Compile(ast.op, expr_l->type, expr_r->type);
                return std::make_unique<Expr>(Expr::DataOp2{.expr_l = std::move(expr_l),
                                                            .expr_r = std::move(expr_r),
                                                            .op     = ast.op},
                                              output_type);
            },
            [&columns, context](const AstExpr::DataBetween& ast)
            {
                ExprPtr                         expr = CompileExpr(*ast.expr, columns, context);
                ExprPtr                         min  = CompileExpr(*ast.min, columns, context);
                ExprPtr                         max  = CompileExpr(*ast.max, columns, context);
                const std::optional<ColumnType> type =
                    CastTogether({expr->type, min->type, max->type}, ast.between_text);
                if (type && !ColumnTypeIsComparable(*type))
                {
                    throw ClientError{"incompatible operand types", ast.between_text};
                }
                if (expr->type && type && *expr->type != *type)
                {
                    expr = std::make_unique<Expr>(
                        Expr::DataCast{.expr = std::move(expr), .to = *type}, *type);
                }
                if (min->type && type && *min->type != *type)
                {
                    min = std::make_unique<Expr>(
                        Expr::DataCast{.expr = std::move(min), .to = *type}, *type);
                }
                if (max->type && type && *max->type != *type)
                {
                    max = std::make_unique<Expr>(
                        Expr::DataCast{.expr = std::move(max), .to = *type}, *type);
                }
                return std::make_unique<Expr>(Expr::DataBetween{.expr         = std::move(expr),
                                                                .min          = std::move(min),
                                                                .max          = std::move(max),
                                                                .negated      = ast.negated,
                                                                .between_text = ast.between_text},
                                              ColumnType::BOOLEAN);
            },
            [&columns, context](const AstExpr::DataIn& ast)
            {
                ExprPtr              expr = CompileExpr(*ast.expr, columns, context);
                std::vector<ExprPtr> list;
                std::vector<std::optional<ColumnType>> types;
                for (const AstExprPtr& ast_element : ast.list)
                {
                    ExprPtr element = CompileExpr(*ast_element, columns, context);
                    types.push_back(element->type);
                    list.push_back(std::move(element));
                }
                const std::optional<ColumnType> type = CastTogether(types, ast.in_text);
                if (expr->type && type && *expr->type != *type)
                {
                    expr = std::make_unique<Expr>(
                        Expr::DataCast{.expr = std::move(expr), .to = *type}, type);
                }
                for (ExprPtr& element : list)
                {
                    if (element->type && type && *element->type != *type)
                    {
                        element = std::make_unique<Expr>(
                            Expr::DataCast{.expr = std::move(element), .to = *type}, type);
                    }
                }
                return std::make_unique<Expr>(Expr::DataIn{.expr    = std::move(expr),
                                                           .list    = std::move(list),
                                                           .negated = ast.negated},
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
                    arg = CompileExpr(
                        *ast.arg, columns,
                        ExprContext{.nonaggregated_columns = context->nonaggregated_columns,
                                    .aggregates            = context->aggregates,
                                    .inside_aggregation    = true});
                    switch (ast.function)
                    {
                    case Function::AVG:
                    case Function::SUM:
                        if (arg->type && !ColumnTypeIsArithmetic(*arg->type))
                        {
                            throw ClientError{"invalid argument type", ast.arg->text};
                        }
                        type = arg->type;
                        break;
                    case Function::MAX:
                    case Function::MIN:
                        if (arg->type && !ColumnTypeIsComparable(*arg->type))
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
                context->aggregates.exprs.push_back(
                    {.function = ast.function, .arg = std::move(arg)});
                return std::make_unique<Expr>(Expr::DataFunction{column_id}, type);
            },
        },
        ast.data);
}

[[nodiscard]] static std::pair<SourcePtr, Columns> CompileSource(const AstSource& ast)
{
    return std::visit(
        Overload{
            [](const AstSource::DataTable& ast)
            {
                auto [table_id, table_columns] = catalog::GetTableNamed(ast.name);
                Columns columns{ast.alias.value_or(ast.name), std::move(table_columns)};
                return std::make_pair(
                    std::make_unique<Source>(Source::DataTable{table_id}, columns.GetType()),
                    std::move(columns));
            },
            [](const AstSource::DataJoinCross& ast)
            {
                auto [source_l, columns_l] = CompileSource(*ast.source_l);
                auto [source_r, columns_r] = CompileSource(*ast.source_r);
                Columns columns{columns_l, columns_r};
                return std::make_pair(
                    std::make_unique<Source>(Source::DataJoinCross{.source_l = std::move(source_l),
                                                                   .source_r = std::move(source_r)},
                                             columns.GetType()),
                    std::move(columns));
            },
            [](const AstSource::DataJoinConditional& ast)
            {
                auto [source_l, columns_l] = CompileSource(*ast.source_l);
                auto [source_r, columns_r] = CompileSource(*ast.source_r);
                Columns columns{columns_l, columns_r};
                ExprPtr condition = CompileExpr(*ast.condition, columns, std::nullopt);
                if (condition->type != ColumnType::BOOLEAN)
                {
                    throw ClientError{"condition must be boolean", ast.condition->text};
                }
                return std::make_pair(
                    std::make_unique<Source>(
                        Source::DataJoinConditional{
                            .source_l = std::move(source_l),
                            .source_r = std::move(source_r),
                            .join = ast.join.value_or(AstSource::DataJoinConditional::Join::INNER),
                            .condition = std::move(condition)},
                        columns.GetType()),
                    std::move(columns));
            },
        },
        ast.data);
};

[[nodiscard]] static std::pair<SourcePtr, Columns>
CompileSources(const std::vector<AstSourcePtr>& asts)
{
    ASSERT(!asts.empty());
    std::pair<SourcePtr, Columns> result = CompileSource(*asts[0]);
    for (std::size_t i = 1; i < asts.size(); i++)
    {
        std::pair<SourcePtr, Columns> other = CompileSource(*asts[i]);
        Columns                       columns{result.second, other.second};
        result = std::make_pair(
            std::make_unique<Source>(Source::DataJoinCross{.source_l = std::move(result.first),
                                                           .source_r = std::move(other.first)},
                                     columns.GetType()),
            std::move(columns));
    }
    return result;
};

[[nodiscard]] static ExprPtr CompileWhere(const Columns& columns, const AstExprPtr& ast)
{
    if (!ast)
    {
        return ExprPtr{};
    }
    ExprPtr expr = CompileExpr(*ast, columns, std::nullopt);
    if (expr->type != ColumnType::BOOLEAN)
    {
        throw ClientError{"condition must be boolean", ast->text};
    }
    return expr;
}

[[nodiscard]] static Aggregates::GroupBy CompileGroupBy(const Columns&                   columns,
                                                        const std::optional<AstGroupBy>& ast)
{
    Aggregates::GroupBy group_by;
    if (ast)
    {
        for (const auto& [ast_column, text] : ast->columns)
        {
            const ColumnId column_id = columns.GetColumn(ast_column).first;
            group_by.push_back(column_id);
        }
    }
    return group_by;
}

[[nodiscard]] static ExprPtr CompileHaving(const Columns& columns, const AstExprPtr& ast,
                                           ExprContext context)
{
    if (!ast)
    {
        return ExprPtr{};
    }
    ExprPtr expr = CompileExpr(*ast, columns, context);
    if (expr->type != ColumnType::BOOLEAN)
    {
        throw ClientError{"condition must be boolean", ast->text};
    }
    return expr;
}

[[nodiscard]] static std::pair<SelectList, catalog::NamedColumns>
CompileSelectList(const Columns& columns, const AstSelectList& ast,
                  std::unordered_map<ColumnId, SourceText>& nonaggregated_columns,
                  Aggregates&                               aggregates)
{
    const catalog::NamedColumns column_names = columns.GetTableColumns();
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
                        const auto& [column_name, column_type] = column_names[column_id.Get()];
                        ExprPtr expr                           = CreateNonaggregatedColumnExpr(
                            column_id, column_type, ast_element.asterisk_text,
                            ExprContext{.nonaggregated_columns = nonaggregated_columns,
                                                                  .aggregates            = aggregates,
                                                                  .inside_aggregation    = false});
                        list.exprs.push_back(std::move(expr));
                        list.type.Push(column_type);
                        list.visible_count++;
                        table_columns.emplace_back(column_name, column_type);
                    }
                },
                [&nonaggregated_columns, &aggregates, &columns, &column_names, &list,
                 &table_columns](const AstSelectList::TableWildcard& ast_element)
                {
                    const auto [begin, end] = columns.GetTable(ast_element.table);
                    for (ColumnId column_id{begin}; column_id < end; column_id++)
                    {
                        const auto& [column_name, column_type] = column_names[column_id.Get()];
                        ExprPtr expr                           = CreateNonaggregatedColumnExpr(
                            column_id, column_type, ast_element.asterisk_text,
                            ExprContext{.nonaggregated_columns = nonaggregated_columns,
                                                                  .aggregates            = aggregates,
                                                                  .inside_aggregation    = false});
                        list.exprs.push_back(std::move(expr));
                        list.type.Push(column_type);
                        list.visible_count++;
                        table_columns.emplace_back(column_name, column_type);
                    }
                },
                [&nonaggregated_columns, &aggregates, &columns, &list,
                 &table_columns](const AstSelectList::Expr& ast_element)
                {
                    ExprPtr expr =
                        CompileExpr(*ast_element.expr, columns,
                                    ExprContext{.nonaggregated_columns = nonaggregated_columns,
                                                .aggregates            = aggregates,
                                                .inside_aggregation    = false});
                    const ColumnType column_type = expr->type.value_or(ColumnType::INTEGER);
                    std::string      column_name =
                        ast_element.alias ? ast_element.alias->Get() : ast_element.expr->ToString();
                    const auto iter =
                        std::ranges::find_if(table_columns, [&column_name](const auto& column)
                                             { return column.first == column_name; });
                    if (iter != table_columns.end())
                    {
                        throw ClientError{"column name or alias collision",
                                          ast_element.alias.value_or(ast_element.expr->text)};
                    }
                    list.exprs.push_back(std::move(expr));
                    list.type.Push(column_type);
                    list.visible_count++;
                    table_columns.emplace_back(std::move(column_name), column_type);
                },
            },
            ast_element);
    }
    return std::make_pair(std::move(list), std::move(table_columns));
}

[[nodiscard]] static std::tuple<Select, Columns, catalog::NamedColumns>
CompileSelect(const AstSelect& ast)
{
    auto [source, columns] = CompileSources(ast.sources);
    ExprPtr    where       = CompileWhere(columns, ast.where);
    Aggregates aggregates  = {
         .exprs    = std::vector<Aggregates::Aggregate>{},
         .group_by = CompileGroupBy(columns, ast.group_by),
    };
    std::unordered_map<ColumnId, SourceText> nonaggregated_columns;
    auto [list, table_columns] =
        CompileSelectList(columns, ast.list, nonaggregated_columns, aggregates);
    ExprPtr having = CompileHaving(columns, ast.having,
                                   ExprContext{.nonaggregated_columns = nonaggregated_columns,
                                               .aggregates            = aggregates,
                                               .inside_aggregation    = false});
    if (!aggregates.exprs.empty() && aggregates.group_by.empty() && !nonaggregated_columns.empty())
    {
        throw ClientError{"nonaggregated column in aggregation",
                          nonaggregated_columns.begin()->second};
    }
    return std::make_tuple(Select{.source     = std::move(source),
                                  .where      = std::move(where),
                                  .aggregates = std::move(aggregates),
                                  .having     = std::move(having),
                                  .list       = std::move(list)},
                           std::move(columns), std::move(table_columns));
}

[[nodiscard]] static OrderBy CompileOrderBy(const Columns& columns, SelectList& list,
                                            const AstOrderBy& ast)
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
                    const auto [column_id, column_type] = columns.GetColumn(column);
                    ColumnId expr_id{};
                    for (; expr_id < list.exprs.size(); expr_id++)
                    {
                        if (const Expr::DataColumn* expr =
                                std::get_if<Expr::DataColumn>(&list.exprs[expr_id.Get()]->data))
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
                    list.type.Push(column_type);
                    return extra_id;
                },
            },
            ast_column.column);
        order_by.columns.push_back({.column_id = column, .asc = ast_column.asc});
    }
    return order_by;
}

[[nodiscard]] static Iter CreateSourceIter(Source& source)
{
    Type& type = source.type;
    return std::visit(
        Overload{
            [&type](Source::DataTable& source) -> Iter
            {
                return std::make_unique<IterScan>(catalog::GetTableFileIds(source.table_id),
                                                  std::move(type), false);
            },
            [&type](Source::DataJoinCross& source) -> Iter
            {
                Iter iter_l = CreateSourceIter(*source.source_l);
                Iter iter_r = CreateSourceIter(*source.source_r);
                return std::make_unique<IterJoinCross>(std::move(iter_l), std::move(iter_r),
                                                       std::move(type));
            },
            [&type](Source::DataJoinConditional& source) -> Iter
            {
                Iter iter_l = CreateSourceIter(*source.source_l);
                Iter iter_r = CreateSourceIter(*source.source_r);
                return std::make_unique<IterJoinQualified>(std::move(iter_l), std::move(iter_r),
                                                           std::move(source.condition),
                                                           std::move(type));
            },
        },
        source.data);
}

[[nodiscard]] static Iter CreateSelectIter(Select& select)
{
    Iter source = CreateSourceIter(*select.source);
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

[[nodiscard]] static Iter CreateQueryIter(QueryTodo&& query)
{
    Iter iter = CreateSelectIter(query.select);
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

[[nodiscard]] static Query CompileQuery(const AstQuery& ast)
{
    auto [select, columns, table_columns] = CompileSelect(ast.select);
    std::optional<OrderBy> order_by;
    if (ast.order_by)
    {
        order_by = CompileOrderBy(columns, select.list, *ast.order_by);
    }
    QueryTodo query{.select = std::move(select), .order_by = std::move(order_by)};
    return {.columns = std::move(table_columns),
            .iter    = CreateQueryIter(std::move(query)),
            .limit   = ast.limit};
}

[[nodiscard]] static CreateTable CompileCreateTable(AstCreateTable& ast)
{
    std::string name = ast.name.Get(); // TODO: .get() move?
    if (catalog::FindTable(name))
    {
        throw ClientError{"table already exists", std::move(ast.name)};
    }
    return {.name = std::move(name), .columns = std::move(ast.columns)};
}

[[nodiscard]] static DropTable CompileDropTable(AstDropTable& ast)
{
    const auto& name  = ast.name.Get();
    const auto  table = catalog::FindTable(name);
    if (!table.has_value())
    {
        throw ClientError{"table does not exists", std::move(ast.name)};
    }
    return {.table_id = table->first};
}

[[nodiscard]] static InsertValue CompileInsertValue(AstInsertValue& ast)
{
    auto [table_id, type] = catalog::GetTable(ast.table);
    if (ast.exprs.size() != type.Size())
    {
        throw ClientError{"column number mismatch"};
    }
    Value   value;
    Columns columns;
    for (std::size_t i = 0; i < type.Size(); i++)
    {
        ExprPtr expr = CompileExpr(*ast.exprs[i], columns, std::nullopt);
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        if (expr->type.has_value() && expr->type.value() != type.At(i))
        {
            throw ClientError{"column type mismatch", ast.exprs[i]->text};
        }
        value.push_back(expr->Eval(nullptr));
    }
    return {.table_id = table_id, .type = std::move(type), .value = std::move(value)};
}

[[nodiscard]] static Statement CompileDelete(const AstDelete& ast)
{
    auto [table_id, table_columns] = catalog::GetTableNamed(ast.table);
    if (ast.condition_opt)
    {
        const Columns columns{ast.table, table_columns};
        auto          condition = CompileExpr(*ast.condition_opt, columns, std::nullopt);
        if (condition->type != ColumnType::BOOLEAN)
        {
            throw ClientError{"condition must be boolean", ast.condition_opt->text};
        }
        const auto file_ids  = catalog::GetTableFileIds(table_id);
        auto       type      = catalog::GetTypeFromNamedColumns(table_columns);
        auto       iter_scan = std::make_unique<IterScan>(file_ids, std::move(type), true);
        auto iter_filter = std::make_unique<IterFilter>(std::move(iter_scan), std::move(condition));
        return DeleteConditional{.table_id = table_id, .iter = std::move(iter_filter)};
    }
    return TruncateTable{.table_id = table_id};
}

[[nodiscard]] Statement CompileStatement(AstStatement& ast)
{
    return std::visit(
        Overload{[](AstCreateTable& ast) -> Statement { return CompileCreateTable(ast); },
                 [](AstDropTable& ast) -> Statement { return CompileDropTable(ast); },
                 [](AstInsertValue& ast) -> Statement { return CompileInsertValue(ast); },
                 [](AstQuery& ast) -> Statement { return CompileQuery(ast); },
                 [](AstUpdate&) -> Statement { UNREACHABLE(); },
                 [](AstDelete& ast) -> Statement { return CompileDelete(ast); }},
        ast);
}
