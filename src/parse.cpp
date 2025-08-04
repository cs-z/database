#include <algorithm>

#include "parse.hpp"

static std::pair<ColumnType, SourceText> parse_type(Lexer &lexer)
{
	if (lexer.accept(Token::KwInteger)) {
		const SourceText text = lexer.step_token().get_text();
		return { ColumnType::INTEGER, text };
	}
	if (lexer.accept(Token::KwReal)) {
		const SourceText text = lexer.step_token().get_text();
		return { ColumnType::REAL, text };
	}
	if (lexer.accept(Token::KwVarchar)) {
		const SourceText text = lexer.step_token().get_text();
		return { ColumnType::VARCHAR, text };
	}
	lexer.unexpected();
}

ColumnType parse_type(const std::string &name)
{
	Lexer lexer { name };
	auto [type, text_unused] = parse_type(lexer);
	lexer.expect(Token::End);
	return type;
}

static std::pair<AstExpr::DataColumn, SourceText> parse_column(Lexer &lexer)
{
	SourceText name = lexer.expect_step(Token::Identifier).get_text();
	if (lexer.accept_step(Token::Period)) {
		SourceText column = lexer.expect_step(Token::Identifier).get_text();
		SourceText text = name + column;
		return { { std::move(name), std::move(column) }, std::move(column) };
	}
	return { { std::nullopt, name }, std::move(name) };
}

using WildcardOrColumn = std::variant<AstSelectList::Wildcard, AstSelectList::TableWildcard, AstExprPtr>;
static std::optional<WildcardOrColumn> parse_wildcard_or_column(Lexer &lexer)
{
	if (lexer.accept(Token::Asterisk)) {
		SourceText asterisk_text = lexer.step_token().get_text();
		return { AstSelectList::Wildcard { std::move(asterisk_text) } };
	}
	if (lexer.accept(Token::Identifier)) {
		SourceText name = lexer.step_token().get_text();
		if (lexer.accept_step(Token::Period)) {
			if (lexer.accept(Token::Asterisk)) {
				SourceText asterisk_text = lexer.step_token().get_text();
				return { AstSelectList::TableWildcard { std::move(name), std::move(asterisk_text) } };
			}
			if (lexer.accept(Token::Identifier)) {
				SourceText column = lexer.step_token().get_text();
				SourceText text = name + column;
				return std::make_unique<AstExpr>(AstExpr::DataColumn { std::move(name), std::move(column) }, std::move(text));
			}
			lexer.unexpected();
		}
		return std::make_unique<AstExpr>(AstExpr::DataColumn { std::nullopt, name }, std::move(name));
	}
	return std::nullopt;
}

struct ExprContext
{
	bool accept_aggregate;
	bool inside_aggregate;
};

static AstExprPtr parse_expr(Lexer &lexer, ExprContext context, int prec = 0, AstExprPtr primary = {});

static AstExprPtr parse_expr_primary(Lexer &lexer, ExprContext context, int prec)
{
	if (lexer.accept(Token::LParen)) {
		SourceText lparen_text = lexer.step_token().get_text();
		AstExprPtr expr = parse_expr(lexer, context);
		SourceText rparen_text = lexer.expect_step(Token::RParen).get_text();
		expr->text = lparen_text + rparen_text;
		return expr;
	}
	if (lexer.accept(Token::KwNull)) {
		SourceText text = lexer.step_token().get_text();
		return std::make_unique<AstExpr>(AstExpr::DataConstant { ColumnValueNull {} }, std::move(text));
	}
	if (lexer.accept(Token::Constant)) {
		auto [value, text] = lexer.step_token().take<Token::DataConstant>();
		return std::make_unique<AstExpr>(AstExpr::DataConstant { std::move(value) }, text);
	}
	if (lexer.accept(Token::Identifier)) {
		auto [column, text] = parse_column(lexer);
		return std::make_unique<AstExpr>(std::move(column), text);
	}
	if (lexer.accept(Token::KwCast)) {
		SourceText cast_text = lexer.step_token().get_text();
		lexer.expect_step(Token::LParen);
		AstExprPtr expr = parse_expr(lexer, context);
		lexer.expect_step(Token::KwAs);
		std::pair<ColumnType, SourceText> to = parse_type(lexer);
		SourceText rparen_text = lexer.expect_step(Token::RParen).get_text();
		return std::make_unique<AstExpr>(AstExpr::DataCast { std::move(expr), to }, cast_text + rparen_text);
	}
	if (prec <= op1_prec(Op1::Pos) && lexer.accept(Token::Op2) && lexer.get_token().get_data<Token::DataOp2>() == Op2::ArithAdd) {
		SourceText op_text = lexer.step_token().get_text();
		AstExprPtr expr = parse_expr(lexer, context, op1_prec(Op1::Pos));
		SourceText text = op_text + expr->text;
		return std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { Op1::Pos, std::move(op_text) } }, std::move(text));
	}
	if (prec <= op1_prec(Op1::Neg) && lexer.accept(Token::Op2) && lexer.get_token().get_data<Token::DataOp2>() == Op2::ArithSub) {
		SourceText op_text = lexer.step_token().get_text();
		AstExprPtr expr = parse_expr(lexer, context, op1_prec(Op1::Neg));
		SourceText text = op_text + expr->text;
		return std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { Op1::Neg, std::move(op_text) } }, std::move(text));
	}
	if (prec <= op1_prec(Op1::Not) && lexer.accept(Token::KwNot)) {
		SourceText op_text = lexer.step_token().get_text();
		AstExprPtr expr = parse_expr(lexer, context, op1_prec(Op1::Not));
		SourceText text = op_text + expr->text;
		return std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { Op1::Not, std::move(op_text) } }, std::move(text));
	}
	if (lexer.accept(Token::Function)) {
		if (context.inside_aggregate) {
			throw ClientError { "aggregations can not be nested", lexer.get_token().get_text() };
		}
		if (!context.accept_aggregate) {
			throw ClientError { "aggregations are invalid here", lexer.get_token().get_text() };
		}
		auto [function, function_text] = lexer.step_token().take<Token::DataFunction>();
		lexer.expect_step(Token::LParen);
		AstExprPtr arg;
		if (lexer.accept(Token::Asterisk)) {
			SourceText arg_text = lexer.step_token().get_text();
			if(function != Function::COUNT) {
				throw ClientError { "invalid argument", std::move(arg_text) };
			}
		}
		else {
			arg = parse_expr(lexer, ExprContext { false, true });
		}
		SourceText rparen_text = lexer.expect_step(Token::RParen).get_text();
		return std::make_unique<AstExpr>(AstExpr::DataFunction { function, std::move(arg) }, function_text + rparen_text);
	}
	lexer.unexpected();
}

static AstExprPtr parse_expr(Lexer &lexer, ExprContext context, int prec, AstExprPtr primary)
{
	ASSERT(!primary || prec == 0);
	AstExprPtr expr = primary ? std::move(primary) : parse_expr_primary(lexer, context, prec);
	for (;;) {
		if (lexer.accept(Token::Op2) || lexer.accept(Token::Asterisk)) {
			const Op2 op = lexer.get_token().get_tag() == Token::Op2 ? lexer.get_token().get_data<Token::DataOp2>() : Op2::ArithMul;
			const int prec_new = op2_prec(op);
			if (prec_new < prec) {
				break;
			}
			SourceText op_text = lexer.step_token().get_text();
			AstExprPtr other = parse_expr(lexer, context, prec_new + 1);
			SourceText text = expr->text + other->text;
			expr = std::make_unique<AstExpr>(AstExpr::DataOp2 { std::move(expr), std::move(other), { op, std::move(op_text) } }, std::move(text));
			continue;
		}
		if (prec <= op1_prec(Op1::IsNull) && lexer.accept(Token::KwIs)) {
			SourceText is_text = lexer.step_token().get_text();
			const bool negated = lexer.accept_step(Token::KwNot);
			SourceText null_text = lexer.expect_step(Token::KwNull).get_text();
			SourceText op_text = is_text + null_text;
			SourceText text = expr->text + null_text;
			expr = std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { negated ? Op1::IsNotNull : Op1::IsNull, std::move(op_text) } }, std::move(text));
			continue;
		}
		if (prec <= op1_prec(Op1::IsNull) && (lexer.accept(Token::KwNot) || lexer.accept(Token::KwBetween) || lexer.accept(Token::KwIn))) {
			const bool negated = lexer.accept_step(Token::KwNot);
			if (lexer.accept(Token::KwBetween)) {
				SourceText between_text = lexer.step_token().get_text();
				AstExprPtr min = parse_expr(lexer, context, op2_prec(Op2::LogicAnd) + 1);
				if (!lexer.accept(Token::Op2) || lexer.get_token().get_data<Token::DataOp2>() != Op2::LogicAnd) {
					throw ClientError { std::string { "expected " } + op2_cstr(Op2::LogicAnd), lexer.get_token().get_text() };
				}
				lexer.step_token();
				AstExprPtr max = parse_expr(lexer, context, op2_prec(Op2::LogicAnd) + 1);
				SourceText text = expr->text + max->text;
				expr = std::make_unique<AstExpr>(AstExpr::DataBetween { std::move(expr), std::move(min), std::move(max), negated, std::move(between_text) }, std::move(text));
				continue;
			}
			if (lexer.accept(Token::KwIn)) {
				SourceText in_text = lexer.step_token().get_text();
				lexer.expect_step(Token::LParen);
				std::vector<AstExprPtr> list;
				do {
					list.push_back(parse_expr(lexer, context));
				} while (lexer.accept_step(Token::Comma));
				SourceText rparen_text = lexer.expect_step(Token::RParen).get_text();
				SourceText text = expr->text + rparen_text;
				expr = std::make_unique<AstExpr>(AstExpr::DataIn { std::move(expr), std::move(list), negated, std::move(in_text) }, std::move(text));
				continue;
			}
			UNREACHABLE();
		}
		break;
	}
	return expr;
}

static AstSelectList::Expr parse_select_list_element_expr(Lexer &lexer, AstExprPtr primary = {})
{
	AstExprPtr expr = parse_expr(lexer, ExprContext { true, false }, 0, std::move(primary));
	std::optional<SourceText> alias;
	if (lexer.accept_step(Token::KwAs)) {
		alias = lexer.expect_step(Token::Identifier).get_text();
	}
	return { std::move(expr), std::move(alias) };
}

static AstSelectList::Element parse_select_list_element(Lexer &lexer)
{
	std::optional<WildcardOrColumn> result = parse_wildcard_or_column(lexer);
	if (result) {
		return std::visit(Overload{
			[](AstSelectList::Wildcard &element) -> AstSelectList::Element {
				return element;
			},
			[](AstSelectList::TableWildcard &element) -> AstSelectList::Element {
				return element;
			},
			[&lexer](AstExprPtr &element) -> AstSelectList::Element {
				return parse_select_list_element_expr(lexer, std::move(element));
			},
		}, *result);
	}
	return parse_select_list_element_expr(lexer);
}

static AstSelectList parse_select_list(Lexer &lexer)
{
	std::vector<AstSelectList::Element> elements;
	do {
		elements.push_back(parse_select_list_element(lexer));
	} while (lexer.accept_step(Token::Comma));
	return { std::move(elements) };
}

static AstSourcePtr parse_source(Lexer &lexer);

static AstSourcePtr parse_source_primary(Lexer &lexer)
{
	if (lexer.accept(Token::Identifier)) {
		SourceText table = lexer.step_token().get_text();
		std::optional<SourceText> alias;
		if (lexer.accept(Token::Identifier) || lexer.accept_step(Token::KwAs)) {
			alias = lexer.expect_step(Token::Identifier).get_text();
		}
		SourceText text = alias ? table + *alias : table;
		return std::make_unique<AstSource>(AstSource::DataTable { std::move(table), std::move(alias) }, std::move(text) );
	}
	if (lexer.accept_step(Token::LParen)) {
		AstSourcePtr source = parse_source(lexer);
		lexer.expect_step(Token::RParen);
		return source;
	}
	lexer.unexpected();
}

static AstSourcePtr parse_source(Lexer &lexer)
{
	AstSourcePtr source_l = parse_source_primary(lexer);
	for (;;) {
		if (lexer.accept_step(Token::KwCross)) {
			lexer.expect_step(Token::KwJoin);
			AstSourcePtr source_r = parse_source_primary(lexer);
			SourceText text = source_l->text + source_r->text;
			source_l = std::make_unique<AstSource>(AstSource::DataJoinCross { std::move(source_l), std::move(source_r) }, std::move(text));
			continue;
		}
		if (lexer.accept(Token::KwJoin) || lexer.accept(Token::KwInner) || lexer.accept(Token::KwLeft) || lexer.accept(Token::KwRight) || lexer.accept(Token::KwFull)) {
			SourceText join_text = lexer.get_token().get_text();
			std::optional<AstSource::DataJoinConditional::Join> join;
			if (lexer.accept_step(Token::KwInner)) {
				join = AstSource::DataJoinConditional::Join::INNER;
			}
			else if (lexer.accept_step(Token::KwLeft)) {
				lexer.accept_step(Token::KwOuter);
				join = AstSource::DataJoinConditional::Join::LEFT;
			}
			else if (lexer.accept_step(Token::KwRight)) {
				lexer.accept_step(Token::KwOuter);
				join = AstSource::DataJoinConditional::Join::RIGHT;
			}
			else if (lexer.accept_step(Token::KwFull)) {
				lexer.accept_step(Token::KwOuter);
				join = AstSource::DataJoinConditional::Join::FULL;
			}
			lexer.expect_step(Token::KwJoin);
			AstSourcePtr source_r = parse_source_primary(lexer);
			lexer.expect_step(Token::KwOn);
			AstExprPtr condition = parse_expr(lexer, ExprContext { false, false });
			SourceText text = join_text + condition->text;
			source_l = std::make_unique<AstSource>(AstSource::DataJoinConditional { std::move(source_l), std::move(source_r), join, std::move(condition) }, std::move(text));
			continue;
		}
		break;
	}
	return source_l;
}

static std::vector<AstSourcePtr> parse_sources(Lexer &lexer)
{
	lexer.expect_step(Token::KwFrom);
	std::vector<AstSourcePtr> sources;
	do {
		sources.push_back(parse_source(lexer));
	} while (lexer.accept_step(Token::Comma));
	return sources;
}

static std::optional<AstGroupBy> parse_group_by(Lexer &lexer)
{
	if (lexer.accept_step(Token::KwGroup)) {
		lexer.expect_step(Token::KwBy);
		std::vector<std::pair<AstExpr::DataColumn, SourceText>> columns;
		do {
			columns.push_back(parse_column(lexer));
		} while (lexer.accept_step(Token::Comma));
		return { { std::move(columns) } };
	}
	return std::nullopt;
}

static AstExprPtr parse_having(Lexer &lexer)
{
	if (lexer.accept_step(Token::KwHaving)) {
		return parse_expr(lexer, ExprContext { true, false });
	}
	return AstExprPtr {};
}

static AstSelect parse_select(Lexer &lexer)
{
	lexer.expect_step(Token::KwSelect);
	AstSelectList list = parse_select_list(lexer);
	std::vector<AstSourcePtr> sources = parse_sources(lexer);
	AstExprPtr where;
	if (lexer.accept_step(Token::KwWhere)) {
		where = parse_expr(lexer, ExprContext { false, false });
	}
	std::optional<AstGroupBy> group_by = parse_group_by(lexer);
	AstExprPtr having = parse_having(lexer);
	return { std::move(list), std::move(sources), std::move(where), std::move(group_by), std::move(having) };
}

static std::variant<AstOrderBy::Index, AstExpr::DataColumn> parse_order_by_column(Lexer &lexer)
{
	const ColumnValueInteger *index;
	if (lexer.accept(Token::Constant) && (index = std::get_if<ColumnValueInteger>(&lexer.get_token().get_data<Token::DataConstant>()))) {
		SourceText text = lexer.step_token().get_text();
		return AstOrderBy::Index { { static_cast<ColumnId>(*index), std::move(text) } };
	}
	else if (lexer.accept(Token::Identifier)) {
		auto [column, column_text_unused] = parse_column(lexer);
		return column;
	}
	lexer.unexpected();
}

static bool parse_order_by_order(Lexer &lexer)
{
	if (lexer.accept_step(Token::KwAsc)) {
		return true;
	}
	if (lexer.accept_step(Token::KwDesc)) {
		return false;
	}
	return true;
}

static std::optional<AstOrderBy> parse_order_by(Lexer &lexer)
{
	if (lexer.accept_step(Token::KwOrder)) {
		lexer.expect_step(Token::KwBy);
		std::vector<AstOrderBy::Column> columns;
		do {
			std::variant<AstOrderBy::Index, AstExpr::DataColumn> column = parse_order_by_column(lexer);
			const bool asc = parse_order_by_order(lexer);
			columns.push_back({ std::move(column), asc });
		} while (lexer.accept_step(Token::Comma));
		return { { std::move(columns) } };
	}
	return std::nullopt;
}

static std::optional<unsigned int> parse_limit(Lexer &lexer)
{
	if (lexer.accept_step(Token::KwLimit)) {
		const ColumnValueInteger *index;
		if (lexer.accept(Token::Constant) && (index = std::get_if<ColumnValueInteger>(&lexer.get_token().get_data<Token::DataConstant>()))) {
			const unsigned int limit = *index;
			lexer.step_token();
			return limit;
		}
		lexer.unexpected();
	}
	return std::nullopt;
}

static AstQuery parse_query(Lexer &lexer)
{
	AstSelect select = parse_select(lexer);
	std::optional<AstOrderBy> order_by = parse_order_by(lexer);
	std::optional<unsigned int> limit = parse_limit(lexer);
	return { std::move(select), std::move(order_by), limit };
}

static AstCreateTable parse_create_table(Lexer &lexer)
{
	lexer.expect_step(Token::KwCreate);
	lexer.expect_step(Token::KwTable);
	SourceText name = lexer.expect_step(Token::Identifier).get_text();
	lexer.expect_step(Token::LParen);
	catalog::TableDef table_def;
	do {
		SourceText column_name = lexer.expect_step(Token::Identifier).get_text();
		const auto iter = std::find_if(table_def.begin(), table_def.end(), [&column_name](const catalog::ColumnDef &column) {
			return column.name == column_name.get();
		});
		if (iter != table_def.end()) {
			throw ClientError { "column name reused", std::move(column_name) };
		}
		auto [column_type, unused] = parse_type(lexer);
		table_def.push_back({ column_name.get(), column_type });
	} while (lexer.accept_step(Token::Comma) && !lexer.accept(Token::RParen));
	lexer.expect_step(Token::RParen);
	return { std::move(name), std::move(table_def) };
}

static AstInsertValue parse_insert_value(Lexer &lexer)
{
	lexer.expect_step(Token::KwInsert);
	lexer.expect_step(Token::KwInto);
	SourceText table = lexer.expect_step(Token::Identifier).get_text();
	lexer.expect_step(Token::KwValues);
	lexer.expect_step(Token::LParen);
	std::vector<AstExprPtr> exprs;
	do {
		exprs.push_back(parse_expr(lexer, ExprContext { false, false }));
	} while (lexer.accept_step(Token::Comma));
	lexer.expect_step(Token::RParen);
	return { std::move(table), std::move(exprs) };
}

AstStatement parse_statement(Lexer &lexer)
{
	if (lexer.accept(Token::KwCreate)) {
		return parse_create_table(lexer);
	}
	if (lexer.accept(Token::KwInsert)) {
		return parse_insert_value(lexer);
	}
	if (lexer.accept(Token::KwSelect)) {
		return parse_query(lexer);
	}
	lexer.unexpected();
}
