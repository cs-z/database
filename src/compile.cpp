#include <algorithm>

#include "compile.hpp"
#include "catalog.hpp"

class Columns
{
public:

	Columns(AstIdentifier table, catalog::TableDef table_def);
	Columns(const Columns &columns_l, const Columns &columns_r);
	Columns(const Columns &other) : tables { other.tables }, columns { other.columns }, columns_table { other.columns_table } {}

	std::pair<row::ColumnId, row::ColumnId> get_table(const AstIdentifier &name) const;
	std::pair<row::ColumnId, ColumnType> get_column(const AstExpr::DataColumn &column) const;

	catalog::TableDef get_table_def() const;

	row::Type get_type() const
	{
		row::Type type;
		for (const catalog::ColumnDef &column_def : columns) {
			type.push_back(column_def.type);
		}
		return type;
	}

private:

	std::optional<row::ColumnId> find_table(const std::string &name) const;

	struct Table
	{
		AstIdentifier name;
		std::pair<row::ColumnId, row::ColumnId> columns;
	};

	std::vector<Table> tables;
	std::vector<catalog::ColumnDef> columns;
	std::vector<std::string> columns_table;
};

Columns::Columns(AstIdentifier table, catalog::TableDef table_def)
{
	for (catalog::ColumnDef &column_def : table_def) {
		columns.push_back(std::move(column_def));
		columns_table.push_back(table.first);
	}
	tables.push_back({ std::move(table), { row::ColumnId {}, row::ColumnId (table_def.size()) } });
}

Columns::Columns(const Columns &columns_l, const Columns &columns_r)
{
	tables = columns_l.tables;
	const row::ColumnId offset = tables.back().columns.second;
	for (const Table &table : columns_r.tables) {
		if (find_table(table.name.first)) {
			throw ClientError { "table name or alias collision", table.name.second };
		}
		const std::pair columns { table.columns.first + offset, table.columns.second + offset };
		tables.push_back({ table.name, columns });
	}
	columns = columns_l.columns;
	for (const catalog::ColumnDef &column_def : columns_r.columns) {
		columns.push_back(column_def);
	}
	columns_table = columns_l.columns_table;
	for (const std::string &table : columns_r.columns_table) {
		columns_table.push_back(table);
	}
}

std::pair<row::ColumnId, row::ColumnId> Columns::get_table(const AstIdentifier &name) const
{
	const std::optional<row::ColumnId> id = find_table(name.first);
	if (!id) {
		throw ClientError { "table not found", name.second };
	}
	return tables[id->get()].columns;
}

std::pair<row::ColumnId, ColumnType> Columns::get_column(const AstExpr::DataColumn &column) const
{
	std::vector<row::ColumnId> ids;
	if (column.table) {
		const auto [begin, end] = get_table(*column.table);
		for (row::ColumnId column_id { begin }; column_id < end; column_id++) {
			if (columns[column_id.get()].name == column.name.first) {
				ids.push_back(column_id);
			}
		}
	}
	else {
		for (row::ColumnId column_id {}; column_id < columns.size(); column_id++) {
			if (columns[column_id.get()].name == column.name.first) {
				ids.push_back(column_id);
			}
		}
	}
	if (ids.size() == 0) {
		throw ClientError { "column not found", column.name.second };
	}
	if (ids.size() > 1) {
		throw ClientError { "column name is ambiguous", column.name.second };
	}
	const row::ColumnId id = ids.front();
	return { id, columns[id.get()].type };
}

catalog::TableDef Columns::get_table_def() const
{
	std::unordered_map<std::string, int> multiplicity;
	for (const catalog::ColumnDef &column_def : columns) {
		multiplicity[column_def.name]++;
	}
	catalog::TableDef table_def;
	for (size_t column_id = 0; column_id < columns.size(); column_id++) {
		const auto &[column_name, column_type] = columns[column_id];
		std::string prefix = multiplicity[column_name] > 1 ? (columns_table[column_id] + ".") : "";
		std::string name = std::move(prefix) + column_name;
		table_def.push_back({ std::move(name), column_type });
	}
	return table_def;
}

std::optional<row::ColumnId> Columns::find_table(const std::string &name) const
{
	for (row::ColumnId column_id {}; column_id < tables.size(); column_id++) {
		if (tables[column_id.get()].name.first == name) {
			return column_id;
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
			throw ClientError { "incompatible operand types", text };
		}
	}
	return type;
}

struct ExprContent
{
	std::unordered_map<row::ColumnId, Text> *nonaggregated_columns;
	Aggregates *aggregates;
	bool inside_aggregation;
};

ExprPtr create_nonaggregated_column_expr(row::ColumnId column_id, ColumnType column_type, Text column_text, ExprContent context)
{
	if (context.nonaggregated_columns && !context.nonaggregated_columns->contains(column_id)) {
		context.nonaggregated_columns->insert({ column_id, column_text });
	}
	if (context.aggregates && context.aggregates->group_by.size() > 0) {
		row::ColumnId key_id {};
		while (key_id < context.aggregates->group_by.size() && context.aggregates->group_by.at(key_id.get()) != column_id) {
			key_id++;
		}
		if (key_id == context.aggregates->group_by.size()) {
			throw ClientError { "nonaggregated column is not in group by clause", column_text };
		}
		return std::make_unique<Expr>(Expr::DataColumn { key_id }, column_type);
	}
	return std::make_unique<Expr>(Expr::DataColumn { column_id }, column_type);
}

static ExprPtr compile_expr(const Columns &columns, const AstExpr &ast, ExprContent context)
{
	const Text text = ast.text;
	return std::visit(Overload{
		[](const AstExpr::DataConstant &ast) {
			const std::optional<ColumnType> type = column_value_to_type(ast.value);
			return std::make_unique<Expr>(Expr::DataConstant { std::move(ast.value) }, type);
		},
		[&columns, context, text](const AstExpr::DataColumn &ast) {
			const auto [column_id, column_type] = columns.get_column(ast);
			if (!context.inside_aggregation) {
				return create_nonaggregated_column_expr(column_id, column_type, text, context);
			}
			return std::make_unique<Expr>(Expr::DataColumn { column_id }, column_type);
		},
		[&columns, context](const AstExpr::DataCast &ast) {
			ExprPtr expr = compile_expr(columns, *ast.expr, context);
			compile_cast(expr->type, ast.to);
			return std::make_unique<Expr>(Expr::DataCast { std::move(expr), ast.to.first }, ast.to.first);
		},
		[&columns, context](const AstExpr::DataOp1 &ast) {
			ExprPtr expr = compile_expr(columns, *ast.expr, context);
			const std::optional<ColumnType> type = op1_compile(ast.op, expr->type);
			return std::make_unique<Expr>(Expr::DataOp1 { std::move(expr), ast.op }, type);
		},
		[&columns, context](const AstExpr::DataOp2 &ast) {
			ExprPtr expr_l = compile_expr(columns, *ast.expr_l, context);
			ExprPtr expr_r = compile_expr(columns, *ast.expr_r, context);
			const std::optional<ColumnType> input_type = cast_together({ expr_l->type, expr_r->type }, ast.op.second);
			if (expr_l->type && input_type && *expr_l->type != *input_type) {
				expr_l = std::make_unique<Expr>(Expr::DataCast { std::move(expr_l), *input_type });
			}
			if (expr_r->type && input_type && *expr_r->type != *input_type) {
				expr_r = std::make_unique<Expr>(Expr::DataCast { std::move(expr_r), *input_type });
			}
			const std::optional<ColumnType> output_type = op2_compile(ast.op, expr_l->type, expr_r->type);
			return std::make_unique<Expr>(Expr::DataOp2 { std::move(expr_l), std::move(expr_r), ast.op }, output_type);
		},
		[&columns, context](const AstExpr::DataBetween &ast) {
			ExprPtr expr = compile_expr(columns, *ast.expr, context);
			ExprPtr min = compile_expr(columns, *ast.min, context);
			ExprPtr max = compile_expr(columns, *ast.max, context);
			const std::optional<ColumnType> type = cast_together({expr->type, min->type, max->type}, ast.between_text);
			if (type && !column_type_is_comparable(*type)) {
				throw ClientError { "incompatible operand types", ast.between_text };
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
			return std::make_unique<Expr>(Expr::DataBetween { std::move(expr), std::move(min), std::move(max), ast.negated, ast.between_text }, ColumnType::BOOLEAN);
		},
		[&columns, context](const AstExpr::DataIn &ast) {
			ExprPtr expr = compile_expr(columns, *ast.expr, context);
			std::vector<ExprPtr> list;
			std::vector<std::optional<ColumnType>> types;
			for (const AstExprPtr &ast_element : ast.list) {
				ExprPtr element = compile_expr(columns, *ast_element, context);
				types.push_back(element->type);
				list.push_back(std::move(element));
			}
			const std::optional<ColumnType> type = cast_together(types, ast.in_text);
			if (expr->type && type && *expr->type != *type) {
				expr = std::make_unique<Expr>(Expr::DataCast { std::move(expr), *type });
			}
			for (ExprPtr &element : list) {
				if (element->type && type && *element->type != *type) {
					element = std::make_unique<Expr>(Expr::DataCast { std::move(element), *type });
				}
			}
			return std::make_unique<Expr>(Expr::DataIn { std::move(expr), std::move(list), ast.negated }, ColumnType::BOOLEAN);
		},
		[&columns, context](const AstExpr::DataFunction &ast) {
			ASSERT(context.aggregates);
			ASSERT(!context.inside_aggregation);
			ExprPtr arg;
			std::optional<ColumnType> type;
			if (ast.arg) {
				arg = compile_expr(columns, *ast.arg, ExprContent { context.nonaggregated_columns, context.aggregates, true });
				switch (ast.function) {
					case Function::AVG:
					case Function::SUM:
						if (arg->type && !column_type_is_arithmetic(*arg->type)) {
							throw ClientError { "invalid argument type", ast.arg->text };
						}
						type = arg->type;
						break;
					case Function::MAX:
					case Function::MIN:
						if (arg->type && !column_type_is_comparable(*arg->type)) {
							throw ClientError { "invalid argument type", ast.arg->text };
						}
						type = arg->type;
						break;
					case Function::COUNT:
						type = ColumnType::INTEGER;
						break;
				}
			}
			else {
				ASSERT(ast.function == Function::COUNT);
				type = ColumnType::INTEGER;
			}
			const row::ColumnId column_id (context.aggregates->group_by.size() + context.aggregates->exprs.size());
			context.aggregates->exprs.push_back({ ast.function, std::move(arg) });
			return std::make_unique<Expr>(Expr::DataFunction { column_id }, type);
		},
	}, ast.data);
}

static std::pair<SourcePtr, Columns> compile_source(const AstSource &ast)
{
	return std::visit(Overload{
		[](const AstSource::DataTable &ast) {
			auto table = catalog::find_table(ast.name.first);
			if (!table) {
				throw ClientError { "table does not exist", ast.name.second };
			}
			Columns columns { ast.alias.value_or(ast.name), std::move(table->second) };
			return std::make_pair(std::make_unique<Source>(Source::DataTable { table->first }, columns.get_type()), std::move(columns));
		},
		[](const AstSource::DataJoinCross &ast) {
			auto [source_l, columns_l] = compile_source(*ast.source_l);
			auto [source_r, columns_r] = compile_source(*ast.source_r);
			Columns columns { std::move(columns_l), std::move(columns_r) };
			return std::make_pair(std::make_unique<Source>(Source::DataJoinCross { std::move(source_l), std::move(source_r) }, columns.get_type()), std::move(columns));
		},
		[](const AstSource::DataJoinConditional &ast) {
			auto [source_l, columns_l] = compile_source(*ast.source_l);
			auto [source_r, columns_r] = compile_source(*ast.source_r);
			Columns columns { std::move(columns_l), std::move(columns_r) };
			ExprPtr condition = compile_expr(columns, *ast.condition, ExprContent { nullptr, nullptr, false });
			if (condition->type != ColumnType::BOOLEAN) {
				throw ClientError { "condition must be boolean", ast.condition->text };
			}
			return std::make_pair(std::make_unique<Source>(Source::DataJoinConditional { std::move(source_l), std::move(source_r), ast.join.value_or(AstSource::DataJoinConditional::Join::INNER), std::move(condition) }, columns.get_type()), std::move(columns));
		},
	}, ast.data);
};

static std::pair<SourcePtr, Columns> compile_sources(const std::vector<AstSourcePtr> &asts)
{
	ASSERT(asts.size() > 0);
	std::pair<SourcePtr, Columns> result = compile_source(*asts[0]);
	for (size_t i = 1; i < asts.size(); i++) {
		std::pair<SourcePtr, Columns> other = compile_source(*asts[i]);
		Columns columns { std::move(result.second), std::move(other.second) };
		result = std::make_pair(std::make_unique<Source>(Source::DataJoinCross { std::move(result.first), std::move(other.first) }, columns.get_type()), std::move(columns));
	}
	return result;
};

static ExprPtr compile_where(const Columns &columns, const AstExprPtr &ast)
{
	if (!ast) {
		return {};
	}
	ExprPtr expr = compile_expr(columns, *ast, ExprContent { nullptr, nullptr, false });
	if (expr->type != ColumnType::BOOLEAN) {
		throw ClientError { "condition must be boolean", ast->text };
	}
	return expr;
}

static Aggregates::GroupBy compile_group_by(const Columns &columns, const std::optional<AstGroupBy> &ast)
{
	Aggregates::GroupBy group_by;
	if (ast) {
		for (const auto &[ast_column, text] : ast->columns) {
			const row::ColumnId column_id = columns.get_column(ast_column).first;
			group_by.push_back(column_id);
		}
	}
	return group_by;
}

static std::pair<SelectList, catalog::TableDef> compile_select_list(const Columns &columns, const AstSelectList &ast, Aggregates &aggregates, std::unordered_map<row::ColumnId, Text> &nonaggregated_columns)
{
	const catalog::TableDef columns_table_def = columns.get_table_def();
	SelectList list = {};
	catalog::TableDef table_def;
	for (const AstSelectList::Element &ast_element : ast.elements) {
		// TODO: capture list too long
		std::visit(Overload{
			[&nonaggregated_columns, &aggregates, &columns, &columns_table_def, &list, &table_def](const AstSelectList::Wildcard &ast_element) {
				for (row::ColumnId column_id {}; column_id < columns_table_def.size(); column_id++) {
					const auto &[column_name, column_type] = columns_table_def[column_id.get()];
					ExprPtr expr = create_nonaggregated_column_expr(column_id, column_type, ast_element.asterisk_text, ExprContent { &nonaggregated_columns, &aggregates, false });
					list.exprs.push_back(std::move(expr));
					list.type.push_back(column_type);
					list.visible_count++;
					table_def.push_back({ column_name, column_type });
				}
			},
			[&nonaggregated_columns, &aggregates, &columns, &columns_table_def, &list, &table_def](const AstSelectList::TableWildcard &ast_element) {
				const auto [begin, end] = columns.get_table(ast_element.table);
				for (row::ColumnId column_id { begin }; column_id < end; column_id++) {
					const auto &[column_name, column_type] = columns_table_def[column_id.get()];
					ExprPtr expr = create_nonaggregated_column_expr(column_id, column_type, ast_element.asterisk_text, ExprContent { &nonaggregated_columns, &aggregates, false });
					list.exprs.push_back(std::move(expr));
					list.type.push_back(column_type);
					list.visible_count++;
					table_def.push_back({ column_name, column_type });
				}
			},
			[&nonaggregated_columns, &aggregates, &columns, &list, &table_def](const AstSelectList::Expr &ast_element) {
				ExprPtr expr = compile_expr(columns, *ast_element.expr, ExprContent { &nonaggregated_columns, &aggregates, false });
				const ColumnType column_type = expr->type.value_or(ColumnType::INTEGER);
				std::string column_name = ast_element.alias ? ast_element.alias->first : ast_element.expr->to_string();
				const auto iter = std::find_if(table_def.begin(), table_def.end(), [&column_name](const catalog::ColumnDef &column_def) {
					return column_def.name == column_name;
				});
				if (iter != table_def.end()) {
					throw ClientError { "column name or alias collision", ast_element.alias ? ast_element.alias->second : ast_element.expr->text };
				}
				list.exprs.push_back(std::move(expr));
				list.type.push_back(column_type);
				list.visible_count++;
				table_def.push_back({ std::move(column_name), column_type });
			},
		}, ast_element);
	}
	return std::make_pair(std::move(list), std::move(table_def));
}

static std::tuple<Select, Columns, catalog::TableDef> compile_select(const AstSelect &ast)
{
	auto [source, columns] = compile_sources(ast.sources);
	ExprPtr where = compile_where(columns, ast.where);
	Aggregates aggregates = {
		std::vector<Aggregates::Aggregate>{},
		compile_group_by(columns, ast.group_by),
	};
	std::unordered_map<row::ColumnId, Text> nonaggregated_columns;
	auto [list, table_def] = compile_select_list(columns, ast.list, aggregates, nonaggregated_columns);
	if (aggregates.exprs.size() > 0 && aggregates.group_by.size() == 0 && nonaggregated_columns.size() > 0) {
		throw ClientError { "nonaggregated column in aggregation", nonaggregated_columns.begin()->second };
	}
	return std::make_tuple(Select { std::move(source), std::move(where), std::move(aggregates), std::move(list) }, std::move(columns), std::move(table_def));
}

static OrderBy compile_order_by(const Columns &columns, SelectList &list, const AstOrderBy &ast)
{
	OrderBy order_by;
	for (const AstOrderBy::Column &ast_column : ast.columns) {
		const row::ColumnId column = std::visit(Overload{
			[&list](const AstOrderBy::Index &column) {
				if (column.index.first <= 0 || column.index.first > list.exprs.size()) {
					throw ClientError { "invalid column index", column.index.second };
				}
				return column.index.first - 1;
			},
			[&columns, &list, &order_by](const AstExpr::DataColumn &column) {
				const auto [column_id, column_type] = columns.get_column(column);
				row::ColumnId expr_id {};
				for (; expr_id < list.exprs.size(); expr_id++) {
					if (const Expr::DataColumn *expr = std::get_if<Expr::DataColumn>(&list.exprs[expr_id.get()]->data)) {
						if (expr->column_id == column_id) {
							break;
						}
					}
				}
				if (expr_id < list.exprs.size()) {
					return expr_id;
				}
				else {
					const row::ColumnId extra_id (list.exprs.size());
					list.exprs.push_back(std::make_unique<Expr>(Expr::DataColumn { column_id }, column_type));
					list.type.push_back(column_type);
					return extra_id;
				}
			},
		}, ast_column.column);
		order_by.columns.push_back({ column, ast_column.asc });
	}
	return order_by;
}

Query compile_query(const AstQuery &ast)
{
	auto [select, columns, table_def] = compile_select(ast.select);
	std::optional<OrderBy> order_by;
	if (ast.order_by) {
		order_by = compile_order_by(columns, select.list, *ast.order_by);
	}
	return { std::move(select), std::move(order_by), std::move(table_def) };
}

//InsertValue compile_insert_value(AstInsertValue &ast)
//{
//
//}
