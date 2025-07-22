#include "ast.hpp"

const std::string AstExpr::DataColumn::to_string() const
{
	return table ? (table->first + "." + name.first) : name.first;
}

static void print_column(const AstExpr::DataColumn &column)
{
	printf("%s", column.to_string().c_str());
}

std::string AstExpr::to_string() const
{
	return std::visit(Overload{
		[](const DataConstant &expr) {
			return column_value_to_string(expr.value, true);
		},
		[](const DataColumn &expr) {
			return expr.to_string();
		},
		[](const DataCast &expr) {
			return "CAST(" + expr.expr->to_string() + " AS " + column_type_to_string(expr.to.first) + ")";
		},
		[](const DataOp1 &expr) {
			return op1_to_string(expr.op.first, expr.expr->to_string());
		},
		[](const DataOp2 &expr) {
			return "(" + expr.expr_l->to_string() + " " + op2_cstr(expr.op.first) + " " + expr.expr_r->to_string() + ")";
		},
		[](const DataBetween &expr) {
			std::string string = expr.expr->to_string();
			if (expr.negated) {
				string += " NOT";
			}
			string += " BETWEEN ";
			string += "(" + expr.min->to_string() + ")";
			string += " AND ";
			string += "(" + expr.max->to_string() + ")";
			return string;
		},
		[](const DataIn &expr) {
			std::string string = expr.expr->to_string();
			if (expr.negated) {
				string += " NOT";
			}
			string += " IN ";
			string += "(";
			for (size_t i = 0; i < expr.list.size(); i++) {
				string += expr.list[i]->to_string();
				if (i + 1 < expr.list.size()) {
					string += ", ";
				}
			}
			string += ")";
			return string;
		},
		[](const DataAggregate &expr) {
			return std::string { aggregate_function_cstr(expr.function) } + "(" + (expr.arg ? expr.arg->to_string() : "*") + ")";
		},
	}, data);
}

void AstExpr::print() const
{
	printf("%s", to_string().c_str());
}

void AstQuery::print() const
{
	printf("SELECT ");
	for (size_t i = 0; i < select.list.elements.size(); i++) {
		std::visit(Overload{
			[](const AstSelectList::Wildcard &) {
				printf("*");
			},
			[](const AstSelectList::TableWildcard &element) {
				printf("%s.*", element.table.first.c_str());
			},
			[](const AstSelectList::Expr &element) {
				element.expr->print();
				if (element.alias) {
					printf(" AS %s", element.alias->first.c_str());
				}
			},
		}, select.list.elements[i]);
		if (i + 1< select.list.elements.size()) {
			printf(", ");
		}
	}
	printf (" FROM ");
	for (size_t i = 0; i < select.sources.size(); i++) {
		select.sources[i]->print();
		if (i + 1< select.sources.size()) {
			printf(", ");
		}
	}
	if (select.where) {
		printf(" WHERE ");
		select.where->print();
	}
	if (select.group_by) {
		printf(" GROUP BY ");
		for (size_t i = 0; i < select.group_by->columns.size(); i++) {
			print_column(select.group_by->columns[i].first);
			if (i + 1< select.group_by->columns.size()) {
				printf(", ");
			}
		}
	}
	if (order_by) {
		printf(" ORDER BY ");
		for (size_t i = 0; i < order_by->columns.size(); i++) {
			std::visit(Overload{
				[](const AstOrderBy::Index &column) {
					printf("%u", column.index.first);
				},
				[](const AstExpr::DataColumn &column) {
					print_column(column);
				},
			}, order_by->columns[i].column);
			printf(" %s", order_by->columns[i].asc ? " ASC" : " DESC");
			if (i + 1 < order_by->columns.size()) {
				printf(", ");
			}
		}
	}
	printf("\n");
}

void AstSource::print() const
{
	std::visit(Overload{
		[](const DataTable &table) {
			printf("%s", table.name.first.c_str());
			if (table.alias) {
				printf(" AS %s", table.alias->first.c_str());
			}
		},
		[](const DataJoinCross &table) {
			printf("(");
			table.source_l->print();
			printf(" CROSS JOIN ");
			table.source_r->print();
			printf(")");
		},
		[](const DataJoinConditional &table) {
			printf("(");
			table.source_l->print();
			if (table.join) {
				switch (*table.join) {
					case DataJoinConditional::Join::INNER:
						printf(" INNER");
						break;
					case DataJoinConditional::Join::LEFT:
						printf(" LEFT OUTER");
						break;
					case DataJoinConditional::Join::RIGHT:
						printf(" RIGHT OUTER");
						break;
					case DataJoinConditional::Join::FULL:
						printf(" FULL OUTER");
						break;
				}
			}
			printf(" JOIN ");
			table.source_r->print();
			printf(" ON ");
			table.condition->print();
			printf(")");
		},
	}, data);
}
