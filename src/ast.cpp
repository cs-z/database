#include "ast.hpp"
#include "common.hpp"
#include "op.hpp"
#include "type.hpp"
#include "value.hpp"

#include <cstdio>
#include <string>
#include <variant>

std::string AstExpr::DataColumn::ToString() const
{
    return table ? (table->Get() + "." + name.Get()) : name.Get();
}

static void PrintColumn(const AstExpr::DataColumn& column)
{
    std::printf("%s", column.ToString().c_str());
}

std::string AstExpr::ToString() const
{
    return std::visit(
        Overload{
            [](const DataConstant& expr) { return ColumnValueToString(expr.value, true); },
            [](const DataColumn& expr) { return expr.ToString(); },
            [](const DataCast& expr) {
                return "CAST(" + expr.expr->ToString() + " AS " +
                       ColumnTypeToString(expr.to.first) + ")";
            },
            [](const DataOp1& expr) { return Op1ToString(expr.op.first, expr.expr->ToString()); },
            [](const DataOp2& expr)
            {
                return "(" + expr.expr_l->ToString() + " " + Op2Cstr(expr.op.first) + " " +
                       expr.expr_r->ToString() + ")";
            },
            [](const DataBetween& expr)
            {
                std::string string = expr.expr->ToString();
                if (expr.negated)
                {
                    string += " NOT";
                }
                string += " BETWEEN ";
                string += "(" + expr.min->ToString() + ")";
                string += " AND ";
                string += "(" + expr.max->ToString() + ")";
                return string;
            },
            [](const DataIn& expr)
            {
                std::string string = expr.expr->ToString();
                if (expr.negated)
                {
                    string += " NOT";
                }
                string += " IN ";
                string += "(";
                for (std::size_t i = 0; i < expr.list.size(); i++)
                {
                    string += expr.list[i]->ToString();
                    if (i + 1 < expr.list.size())
                    {
                        string += ", ";
                    }
                }
                string += ")";
                return string;
            },
            [](const DataFunction& expr)
            {
                return std::string{FunctionToCstr(expr.function)} + "(" +
                       (expr.arg ? expr.arg->ToString() : "*") + ")";
            },
        },
        data);
}

void AstExpr::Print() const
{
    std::printf("%s", ToString().c_str());
}

void AstQuery::Print() const
{
    std::printf("SELECT ");
    for (std::size_t i = 0; i < select.list.elements.size(); i++)
    {
        std::visit(
            Overload{
                [](const AstSelectList::Wildcard&) { std::printf("*"); },
                [](const AstSelectList::TableWildcard& element)
                { std::printf("%s.*", element.table.Get().c_str()); },
                [](const AstSelectList::Expr& element)
                {
                    element.expr->Print();
                    if (element.alias)
                    {
                        std::printf(" AS %s", element.alias->Get().c_str());
                    }
                },
            },
            select.list.elements[i]);
        if (i + 1 < select.list.elements.size())
        {
            std::printf(", ");
        }
    }
    std::printf(" FROM ");
    for (std::size_t i = 0; i < select.sources.size(); i++)
    {
        select.sources[i]->Print();
        if (i + 1 < select.sources.size())
        {
            std::printf(", ");
        }
    }
    if (select.where)
    {
        std::printf(" WHERE ");
        select.where->Print();
    }
    if (select.group_by)
    {
        std::printf(" GROUP BY ");
        for (std::size_t i = 0; i < select.group_by->columns.size(); i++)
        {
            PrintColumn(select.group_by->columns[i].first);
            if (i + 1 < select.group_by->columns.size())
            {
                std::printf(", ");
            }
        }
    }
    if (order_by)
    {
        std::printf(" ORDER BY ");
        for (std::size_t i = 0; i < order_by->columns.size(); i++)
        {
            std::visit(
                Overload{
                    [](const AstOrderBy::Index& column)
                    { std::printf("%u", column.index.first.Get()); },
                    [](const AstExpr::DataColumn& column) { PrintColumn(column); },
                },
                order_by->columns[i].column);
            std::printf(" %s", order_by->columns[i].asc ? " ASC" : " DESC");
            if (i + 1 < order_by->columns.size())
            {
                std::printf(", ");
            }
        }
    }
    if (limit)
    {
        std::printf(" LIMIT %u", *limit);
    }
}

void AstSource::Print() const
{
    std::visit(
        Overload{
            [](const DataTable& table)
            {
                std::printf("%s", table.name.Get().c_str());
                if (table.alias)
                {
                    std::printf(" AS %s", table.alias->Get().c_str());
                }
            },
            [](const DataJoinCross& table)
            {
                std::printf("(");
                table.source_l->Print();
                std::printf(" CROSS JOIN ");
                table.source_r->Print();
                std::printf(")");
            },
            [](const DataJoinConditional& table)
            {
                std::printf("(");
                table.source_l->Print();
                if (table.join)
                {
                    switch (*table.join)
                    {
                    case DataJoinConditional::Join::kInner:
                        std::printf(" INNER");
                        break;
                    case DataJoinConditional::Join::kLeft:
                        std::printf(" LEFT OUTER");
                        break;
                    case DataJoinConditional::Join::kRight:
                        std::printf(" RIGHT OUTER");
                        break;
                    case DataJoinConditional::Join::kFull:
                        std::printf(" FULL OUTER");
                        break;
                    }
                }
                std::printf(" JOIN ");
                table.source_r->Print();
                std::printf(" ON ");
                table.condition->Print();
                std::printf(")");
            },
        },
        data);
}
