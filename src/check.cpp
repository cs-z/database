#include <algorithm>

#include "check.hpp"
#include "catalog.hpp"

Columns::Columns(AstIdentifier table, const Type &type)
{
	tables.push_back({ table, { 0, type.get_column_count() } });
	for (const auto &[column_name, column_type] : type.get_columns()) {
		columns.push_back({ table.first, column_name, column_type });
	}
}

Columns::Columns(Columns columns_l, Columns columns_r)
{
	tables = std::move(columns_l.tables);
	const unsigned int offset = tables.back().columns.second;
	for (Table &table : columns_r.tables) {
		if (find_table(table.name.first)) {
			throw ClientError { "table name or alias collision", table.name.second };
		}
		const std::pair columns { table.columns.first + offset, table.columns.second + offset };
		tables.push_back({ std::move(table.name), columns });
	}
	columns = std::move(columns_l.columns);
	for (Column &column : columns_r.columns) {
		columns.push_back(std::move(column));
	}
}

std::pair<unsigned int, unsigned int> Columns::get_table(const AstIdentifier &name) const
{
	const std::optional<unsigned int> id = find_table(name.first);
	if (!id) {
		throw ClientError { "table not found", name.second };
	}
	return tables[*id].columns;
}

std::pair<unsigned int, ColumnType> Columns::get_column(const AstExpr::DataColumn &column) const
{
	std::vector<unsigned int> ids;
	if (column.table) {
		const auto [begin, end] = get_table(*column.table);
		for (unsigned int i = begin; i < end; i++) {
			if (columns[i].name == column.name.first) {
				ids.push_back(i);
			}
		}
	}
	else {
		for (unsigned int i = 0; i < columns.size(); i++) {
			if (columns[i].name == column.name.first) {
				ids.push_back(i);
			}
		}
	}
	if (ids.size() == 0) {
		throw ClientError { "column not found", column.name.second };
	}
	if (ids.size() > 0) {
		throw ClientError { "column name is ambiguous", column.name.second };
	}
	const unsigned int id = ids.front();
	return { id, columns[id].type };
}

Type Columns::get_type() const
{
	std::unordered_map<std::string, unsigned int> multiplicity;
	for (const Column &column : columns) {
		multiplicity[column.name]++;
	}
	Type type;
	for (const Column &column : columns) {
		std::string name = multiplicity[column.name] > 1 ? (column.table + "." + column.name) : column.name;
		type.add_column(std::move(name), column.type);
	}
	return type;
}

std::optional<unsigned int> Columns::find_table(const std::string &name) const
{
	for (unsigned int i = 0; i < tables.size(); i++) {
		if (tables[i].name.first == name) {
			return i;
		}
	}
	return std::nullopt;
}

static std::optional<ColumnType> cast_together(ColumnType type_l, ColumnType type_r)
{
	if (type_l == type_r) {
		return type_l;
	}
	if (type_l == ColumnType::INTEGER && type_r == ColumnType::REAL) {
		return ColumnType::REAL;
	}
	if (type_l == ColumnType::REAL && type_r == ColumnType::INTEGER) {
		return ColumnType::REAL;
	}
	return std::nullopt;
}

std::optional<ColumnType> cast_together(const std::vector<std::optional<ColumnType>> &list, Text text)
{
	std::optional<ColumnType> type;
	for (std::optional<ColumnType> element : list) {
		if (!element) {
			continue;
		}
		if (!type) {
			type = element;
			continue;
		}
		type = cast_together(*type, *element);
		if (!type) {
			throw ClientError { "incompatible types", text };
		}
	}
	return type;
}

struct ExprContent
{
	std::unordered_map<unsigned int, Text> &nonaggregated_columns;
	Aggregates *aggregates;
	bool inside_aggregation;
};

static std::unique_ptr<Expr> check_expr(const Columns &columns, const AstExpr &ast_expr, ExprContent context)
{
	const Text text = ast_expr.text;
	return std::visit(Overload{
		[](const AstExpr::DataConstant &ast_expr) {
			const std::optional<ColumnType> type = column_value_to_type(ast_expr.value);
			return std::make_unique<Expr>(Expr::DataConstant { std::move(ast_expr.value) }, type);
		},
		[&columns, context, text](const AstExpr::DataColumn &ast_expr) {
			const auto [id, type] = columns.get_column(ast_expr);
			if (!context.inside_aggregation && !context.nonaggregated_columns.contains(id)) {
				context.nonaggregated_columns.insert({ id, text });
			}
			return std::make_unique<Expr>(Expr::DataColumn { id }, type);
		},
		[&columns, context](const AstExpr::DataCast &ast_expr) {
			std::unique_ptr<Expr> expr = check_expr(columns, *ast_expr.expr, context);
			column_type_check_cast(expr->type, ast_expr.to);
			return std::make_unique<Expr>(Expr::DataCast { std::move(expr), ast_expr.to.first }, ast_expr.to.first);
		},
		[&columns, context](const AstExpr::DataOp1 &ast_expr) {
			std::unique_ptr<Expr> expr = check_expr(columns, *ast_expr.expr, context);
			const std::optional<ColumnType> type = op1_check(ast_expr.op, expr->type);
			return std::make_unique<Expr>(Expr::DataOp1 { std::move(expr), ast_expr.op }, type);
		},
		[&columns, context](const AstExpr::DataOp2 &ast_expr) {
			std::unique_ptr<Expr> expr_l = check_expr(columns, *ast_expr.expr_l, context);
			std::unique_ptr<Expr> expr_r = check_expr(columns, *ast_expr.expr_r, context);
			const std::optional<ColumnType> input_type = cast_together({ expr_l->type, expr_r->type }, ast_expr.op.second);
			if (expr_l->type && input_type && *expr_l->type != *input_type) {
				expr_l = std::make_unique<Expr>(Expr::DataCast { std::move(expr_l), *input_type });
			}
			if (expr_r->type && input_type && *expr_r->type != *input_type) {
				expr_r = std::make_unique<Expr>(Expr::DataCast { std::move(expr_r), *input_type });
			}
			const std::optional<ColumnType> output_type = op2_check(ast_expr.op, expr_l->type, expr_r->type);
			return std::make_unique<Expr>(Expr::DataOp2 { std::move(expr_l), std::move(expr_r), ast_expr.op }, output_type);
		},
		[&columns, context](const AstExpr::DataBetween &ast_expr) {
			std::unique_ptr<Expr> expr = check_expr(columns, *ast_expr.expr, context);
			std::unique_ptr<Expr> min = check_expr(columns, *ast_expr.min, context);
			std::unique_ptr<Expr> max = check_expr(columns, *ast_expr.max, context);
			const std::optional<ColumnType> type = cast_together({expr->type, min->type, max->type}, ast_expr.between_text);
			if (type && !column_type_is_comparable(*type)) {
				throw ClientError { "incompatible types", ast_expr.between_text };
			}
			if (expr->type && type && *expr->type != *type) {
				expr = std::make_unique<Expr>(Expr::DataCast { std::move(expr), *type });
			}
			if (min->type && type && *min->type != *type) {
				min = std::make_unique<Expr>(Expr::DataCast { std::move(min), *type });
			}
			if (max->type && type && *max->type != *type) {
				max = std::make_unique<Expr>(Expr::DataCast { std::move(max), *type });
			}
			return std::make_unique<Expr>(Expr::DataBetween { std::move(expr), std::move(min), std::move(max), ast_expr.negated, ast_expr.between_text }, ColumnType::BOOLEAN);
		},
		[&columns, context](const AstExpr::DataIn &ast_expr) {
			std::unique_ptr<Expr> expr = check_expr(columns, *ast_expr.expr, context);
			std::vector<std::unique_ptr<Expr>> list;
			std::vector<std::optional<ColumnType>> types;
			for (const std::unique_ptr<AstExpr> &ast_element : ast_expr.list) {
				std::unique_ptr<Expr> element = check_expr(columns, *ast_element, context);
				types.push_back(element->type);
				list.push_back(std::move(element));
			}
			const std::optional<ColumnType> type = cast_together(types, ast_expr.in_text);
			if (expr->type && type && *expr->type != *type) {
				expr = std::make_unique<Expr>(Expr::DataCast { std::move(expr), *type });
			}
			for (std::unique_ptr<Expr> &element : list) {
				if (element->type && type && *element->type != *type) {
					element = std::make_unique<Expr>(Expr::DataCast { std::move(element), *type });
				}
			}
			return std::make_unique<Expr>(Expr::DataIn { std::move(expr), std::move(list), ast_expr.negated }, ColumnType::BOOLEAN);
		},
		[&columns, context](const AstExpr::DataAggregate &ast_expr) {
			ASSERT(context.aggregates);
			ASSERT(!context.inside_aggregation);
			std::unique_ptr<Expr> arg;
			std::optional<ColumnType> type;
			if (ast_expr.arg) {
				arg = check_expr(columns, *ast_expr.arg, { context.nonaggregated_columns, nullptr, true });
				switch (ast_expr.function) {
					case AggregateFunction::AVG:
					case AggregateFunction::SUM:
						if (arg->type && !column_type_is_arithmetic(*arg->type)) {
							throw ClientError { "invalid argument type", ast_expr.arg->text };
						}
						type = arg->type;
						break;
					case AggregateFunction::MAX:
					case AggregateFunction::MIN:
						if (arg->type && !column_type_is_comparable(*arg->type)) {
							throw ClientError { "invalid argument type", ast_expr.arg->text };
						}
						type = arg->type;
						break;
					case AggregateFunction::COUNT:
						type = ColumnType::INTEGER;
						break;
				}
			}
			else {
				if (ast_expr.function != AggregateFunction::COUNT) {
					throw ClientError { "invalid argument", ast_expr.arg->text };
				}
				type = ColumnType::INTEGER;
			}
			const unsigned int id = context.aggregates->size();
			context.aggregates->push_back({ ast_expr.function, std::move(arg) });
			return std::make_unique<Expr>(Expr::DataAggregate { id }, type);
		},
	}, ast_expr.data);
}

static std::unique_ptr<Source> check_source(const AstSource &ast_source)
{
	return std::visit(Overload{
		[](const AstSource::DataTable &ast_source) {
			const Type *table_type = catalog::find_table(ast_source.name.first);
			if (!table_type) {
				throw ClientError { "table does not exist", ast_source.name.second };
			}
			AstIdentifier table_name = ast_source.alias.value_or(ast_source.name);
			return std::make_unique<Source>(Source::DataTable { ast_source.name.first }, Columns { std::move(table_name), *table_type });
		},
		[](const AstSource::DataJoinCross &ast_source) {
			std::unique_ptr<Source> source_l = check_source(*ast_source.source_l);
			std::unique_ptr<Source> source_r = check_source(*ast_source.source_r);
			Columns columns { source_l->columns, source_r->columns };
			return std::make_unique<Source>(Source::DataJoinCross { std::move(source_l), std::move(source_r) }, std::move(columns));
		},
		[](const AstSource::DataJoinConditional &ast_source) {
			std::unique_ptr<Source> source_l = check_source(*ast_source.source_l);
			std::unique_ptr<Source> source_r = check_source(*ast_source.source_r);
			Columns columns { source_l->columns, source_r->columns };
			std::unordered_map<unsigned int, Text> nonaggregated_columns; // TODO: unused
			std::unique_ptr<Expr> condition = check_expr(columns, *ast_source.condition, { nonaggregated_columns, nullptr, false });
			if (condition->type != ColumnType::BOOLEAN) {
				throw ClientError { "condition must be boolean", ast_source.condition->text };
			}
			return std::make_unique<Source>(Source::DataJoinConditional { std::move(source_l), std::move(source_r), ast_source.join.value_or(AstSource::DataJoinConditional::Join::INNER), std::move(condition) }, std::move(columns));
		},
	}, ast_source.data);
};

static std::unique_ptr<Source> check_sources(const std::vector<std::unique_ptr<AstSource>> &ast_sources)
{
	ASSERT(ast_sources.size() > 0);
	std::unique_ptr<Source> source = check_source(*ast_sources[0]);
	for (size_t i = 1; i < ast_sources.size(); i++) {
		std::unique_ptr<Source> other = check_source(*ast_sources[i]);
		Columns columns { source->columns, other->columns };
		source = std::make_unique<Source>(Source::DataJoinCross { std::move(source), std::move(other) }, std::move(columns));
	}
	return source;
};

static std::unique_ptr<Expr> check_where(const Columns &columns, const std::unique_ptr<AstExpr> &ast_where)
{
	if (!ast_where) {
		return {};
	}
	std::unordered_map<unsigned int, Text> nonaggregated_columns; // TODO: unused
	std::unique_ptr<Expr> expr = check_expr(columns, *ast_where, { nonaggregated_columns, nullptr, false });
	if (expr->type != ColumnType::BOOLEAN) {
		throw ClientError { "condition must be boolean", ast_where->text };
	}
	return expr;
}

static std::optional<GroupBy> check_group_by(const Columns &columns, const std::optional<AstGroupBy> &ast_group_by)
{
	if (!ast_group_by) {
		return std::nullopt;
	}
	std::vector<unsigned int> group_by;
	for (const auto &[ast_column, text] : ast_group_by->columns) {
		const unsigned int id = columns.get_column(ast_column).first;
		group_by.push_back(id);
	}
	return { std::move(group_by) };
}

static SelectList check_select_list(const Columns &columns, const AstSelectList &ast_list)
{
	const Type columns_type = columns.get_type();
	SelectList list;
	for (const AstSelectList::Element &ast_element : ast_list.elements) {
		std::visit(Overload{
			[&columns_type, &list](const AstSelectList::Wildcard &ast_element) {
				for (unsigned int id = 0; id < columns_type.get_column_count(); id++) {
					const auto &[column_name, column_type] = columns_type.get_column(id);
					list.exprs.push_back(std::make_unique<Expr>(Expr::DataColumn { id }));
					list.type.add_column(column_name, column_type);
					if (!list.nonaggregated_columns.contains(id)) {
						list.nonaggregated_columns.insert({ id, ast_element.asterisk_text });
					}
				}
			},
			[&columns, &columns_type, &list](const AstSelectList::TableWildcard &ast_element) {
				const auto [begin, end] = columns.get_table(ast_element.table);
				for (unsigned int id = begin; id < end; id++) {
					const auto &[column_name, column_type] = columns_type.get_column(id);
					list.exprs.push_back(std::make_unique<Expr>(Expr::DataColumn { id }));
					list.type.add_column(column_name, column_type);
					if (!list.nonaggregated_columns.contains(id)) {
						list.nonaggregated_columns.insert({ id, ast_element.asterisk_text });
					}
				}
			},
			[&columns, &list](const AstSelectList::Expr &ast_element) {
				std::unique_ptr<Expr> expr = check_expr(columns, *ast_element.expr, { list.nonaggregated_columns, &list.aggregates, false });
				std::string name = ast_element.alias ? ast_element.alias->first : ast_element.expr->to_string();
				const ColumnType type = expr->type.value_or(ColumnType::INTEGER);
				list.exprs.push_back(std::move(expr));
				list.type.add_column(std::move(name), type);
			},
		}, ast_element);
	}
	if (list.aggregates.size() > 0 && list.nonaggregated_columns.size() > 0) {
		const std::pair<unsigned int, Text> example = *list.nonaggregated_columns.begin();
		throw ClientError { "aggregation without grouping contains unaggregated column", example.second };
	}
	return list;
}

static Select check_select(const AstSelect &ast_select)
{
	std::unique_ptr<Source> source = check_sources(ast_select.sources);
	std::unique_ptr<Expr> where = check_where(source->columns, ast_select.where);
	std::optional<GroupBy> group_by = check_group_by(source->columns, ast_select.group_by);
	SelectList list = check_select_list(source->columns, ast_select.list);
	return { std::move(source), std::move(where), std::move(group_by), std::move(list) };
}

static OrderBy check_order_by(const Type &type, const AstOrderBy &ast_order_by)
{
	OrderBy order_by;
	for (const AstOrderBy::Column &ast_column : ast_order_by.columns) {
		const unsigned int column = std::visit(Overload{
			[&type](const AstOrderBy::Index &column) {
				if (column.index.first >= type.get_column_count()) {
					throw ClientError { "column index is too large", column.index.second };
				}
				return column.index.first;
			},
			[&type](const AstExpr::DataColumn &column) -> unsigned int {
				// TODO
				// const std::optional<unsigned int> id = type.find_column(column.table ? (column.table->first + "." + column.name.first) : column.name.first);
				// ASSERT_CLIENT(id);
				// return *id;
				(void) column;
				throw ClientError { "unimplemented: order by" };
			},
		}, ast_column.column);
		order_by.columns.push_back({ column, ast_column.asc });
	}
	return order_by;
}

Query check_query(const AstQuery &ast_query)
{
	Select select = check_select(ast_query.select);
	std::optional<OrderBy> order_by;
	if (ast_query.order_by) {
		order_by = check_order_by(select.list.type, *ast_query.order_by);
	}
	return { std::move(select), std::move(order_by) };
}
