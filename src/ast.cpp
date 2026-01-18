#include "ast.hpp"

std::string AstExpr::DataColumn::to_string() const
{
    return table ? (table->get() + "." + name.get()) : name.get();
}

static void print_column(const AstExpr::DataColumn& column)
{
    std::printf("%s", column.to_string().c_str());
}

std::string AstExpr::to_string() const
{
    return std::visit(
        Overload{
            [](const DataConstant& expr) { return column_value_to_string(expr.value, true); },
            [](const DataColumn& expr) { return expr.to_string(); },
            [](const DataCast& expr) {
                return "CAST(" + expr.expr->to_string() + " AS " +
                       column_type_to_string(expr.to.first) + ")";
            },
            [](const DataOp1& expr)
            { return op1_to_string(expr.op.first, expr.expr->to_string()); },
            [](const DataOp2& expr)
            {
                return "(" + expr.expr_l->to_string() + " " + op2_cstr(expr.op.first) + " " +
                       expr.expr_r->to_string() + ")";
            },
            [](const DataBetween& expr)
            {
                std::string string = expr.expr->to_string();
                if (expr.negated)
                {
                    string += " NOT";
                }
                string += " BETWEEN ";
                string += "(" + expr.min->to_string() + ")";
                string += " AND ";
                string += "(" + expr.max->to_string() + ")";
                return string;
            },
            [](const DataIn& expr)
            {
                std::string string = expr.expr->to_string();
                if (expr.negated)
                {
                    string += " NOT";
                }
                string += " IN ";
                string += "(";
                for (std::size_t i = 0; i < expr.list.size(); i++)
                {
                    string += expr.list[i]->to_string();
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
                return std::string{function_to_cstr(expr.function)} + "(" +
                       (expr.arg ? expr.arg->to_string() : "*") + ")";
            },
        },
        data);
}

void AstExpr::print() const
{
    std::printf("%s", to_string().c_str());
}

void AstQuery::print() const
{
    std::printf("SELECT ");
    for (std::size_t i = 0; i < select.list.elements.size(); i++)
    {
        std::visit(
            Overload{
                [](const AstSelectList::Wildcard&) { std::printf("*"); },
                [](const AstSelectList::TableWildcard& element)
                { std::printf("%s.*", element.table.get().c_str()); },
                [](const AstSelectList::Expr& element)
                {
                    element.expr->print();
                    if (element.alias)
                    {
                        std::printf(" AS %s", element.alias->get().c_str());
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
        select.sources[i]->print();
        if (i + 1 < select.sources.size())
        {
            std::printf(", ");
        }
    }
    if (select.where)
    {
        std::printf(" WHERE ");
        select.where->print();
    }
    if (select.group_by)
    {
        std::printf(" GROUP BY ");
        for (std::size_t i = 0; i < select.group_by->columns.size(); i++)
        {
            print_column(select.group_by->columns[i].first);
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
                    { std::printf("%u", column.index.first.get()); },
                    [](const AstExpr::DataColumn& column) { print_column(column); },
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

void AstSource::print() const
{
    std::visit(
        Overload{
            [](const DataTable& table)
            {
                std::printf("%s", table.name.get().c_str());
                if (table.alias)
                {
                    std::printf(" AS %s", table.alias->get().c_str());
                }
            },
            [](const DataJoinCross& table)
            {
                std::printf("(");
                table.source_l->print();
                std::printf(" CROSS JOIN ");
                table.source_r->print();
                std::printf(")");
            },
            [](const DataJoinConditional& table)
            {
                std::printf("(");
                table.source_l->print();
                if (table.join)
                {
                    switch (*table.join)
                    {
                    case DataJoinConditional::Join::INNER:
                        std::printf(" INNER");
                        break;
                    case DataJoinConditional::Join::LEFT:
                        std::printf(" LEFT OUTER");
                        break;
                    case DataJoinConditional::Join::RIGHT:
                        std::printf(" RIGHT OUTER");
                        break;
                    case DataJoinConditional::Join::FULL:
                        std::printf(" FULL OUTER");
                        break;
                    }
                }
                std::printf(" JOIN ");
                table.source_r->print();
                std::printf(" ON ");
                table.condition->print();
                std::printf(")");
            },
        },
        data);
}
