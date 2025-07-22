#include "parse.hpp"

static std::pair<ColumnType, Text> parse_type(Lexer &lexer)
{
	if (lexer.accept(Token::KwInteger)) {
		const Text text = lexer.step_token().get_text();
		return { ColumnType::INTEGER, text };
	}
	if (lexer.accept(Token::KwReal)) {
		const Text text = lexer.step_token().get_text();
		return { ColumnType::REAL, text };
	}
	if (lexer.accept(Token::KwVarchar)) {
		const Text text = lexer.step_token().get_text();
		return { ColumnType::VARCHAR, text };
	}
	lexer.unexpect();
}

ColumnType parse_type(std::string name)
{
	Lexer lexer { std::move(name) };
	auto [type, text_unused] = parse_type(lexer);
	lexer.expect(Token::End);
	return type;
}

static std::pair<AstExpr::DataColumn, Text> parse_column(Lexer &lexer)
{
	AstIdentifier name = lexer.expect_step(Token::Identifier).take<Token::DataIdentifier>();
	if (lexer.accept_step(Token::Period)) {
		AstIdentifier column = lexer.expect_step(Token::Identifier).take<Token::DataIdentifier>();
		const Text text = name.second + column.second;
		return { { std::move(name), std::move(column) }, text };
	}
	const Text text = name.second;
	return { { std::nullopt, std::move(name) }, text };
}

using WildcardOrColumn = std::variant<AstSelectList::Wildcard, AstSelectList::TableWildcard, std::unique_ptr<AstExpr>>;
static std::optional<WildcardOrColumn> parse_wildcard_or_column(Lexer &lexer)
{
	if (lexer.accept_step(Token::Asterisk)) {
		return { AstSelectList::Wildcard {} };
	}
	if (lexer.accept(Token::Identifier)) {
		AstIdentifier name = lexer.step_token().take<Token::DataIdentifier>();
		if (lexer.accept_step(Token::Period)) {
			if (lexer.accept_step(Token::Asterisk)) {
				return { AstSelectList::TableWildcard { std::move(name) } };
			}
			if (lexer.accept(Token::Identifier)) {
				AstIdentifier column = lexer.step_token().take<Token::DataIdentifier>();
				const Text text = name.second + column.second;
				return std::make_unique<AstExpr>(AstExpr::DataColumn { std::move(name), std::move(column) }, text);
			}
			lexer.unexpect();
		}
		const Text text = name.second;
		return std::make_unique<AstExpr>(AstExpr::DataColumn { std::nullopt, std::move(name) }, text);
	}
	return std::nullopt;
}

struct ExprContext
{
	bool accept_aggregate;
	bool inside_aggregate;
};

static std::unique_ptr<AstExpr> parse_expr(Lexer &lexer, ExprContext context, unsigned int prec = 0, std::unique_ptr<AstExpr> primary = {});

static std::unique_ptr<AstExpr> parse_expr_primary(Lexer &lexer, ExprContext context, unsigned int prec)
{
	if (lexer.accept(Token::LParen)) {
		const Text lparen_text = lexer.step_token().get_text();
		std::unique_ptr<AstExpr> expr = parse_expr(lexer, context);
		const Text rparen_text = lexer.expect_step(Token::RParen).get_text();
		expr->text = lparen_text + rparen_text;
		return expr;
	}
	if (lexer.accept(Token::KwNull)) {
		const Text text = lexer.step_token().get_text();
		return std::make_unique<AstExpr>(AstExpr::DataConstant { ColumnValueNull {} }, text);
	}
	if (lexer.accept(Token::ConstantBoolean)) {
		auto [value, text] = lexer.step_token().take<Token::DataConstantBoolean>();
		return std::make_unique<AstExpr>(AstExpr::DataConstant { std::move(value.value) }, text);
	}
	if (lexer.accept(Token::ConstantInteger)) {
		auto [value, text] = lexer.step_token().take<Token::DataConstantInteger>();
		return std::make_unique<AstExpr>(AstExpr::DataConstant { std::move(value.value) }, text);
	}
	if (lexer.accept(Token::ConstantReal)) {
		auto [value, text] = lexer.step_token().take<Token::DataConstantReal>();
		return std::make_unique<AstExpr>(AstExpr::DataConstant { std::move(value.value) }, text);
	}
	if (lexer.accept(Token::ConstantString)) {
		auto [value, text] = lexer.step_token().take<Token::DataConstantString>();
		return std::make_unique<AstExpr>(AstExpr::DataConstant { std::move(value.value) }, text);
	}
	if (lexer.accept(Token::Identifier)) {
		auto [column, text] = parse_column(lexer);
		return std::make_unique<AstExpr>(std::move(column), text);
	}
	if (lexer.accept(Token::KwCast)) {
		const Text cast_text = lexer.step_token().get_text();
		lexer.expect_step(Token::LParen);
		std::unique_ptr<AstExpr> expr = parse_expr(lexer, context);
		lexer.expect_step(Token::KwAs);
		const std::pair<ColumnType, Text> to = parse_type(lexer);
		const Text rparen_text = lexer.expect_step(Token::RParen).get_text();
		return std::make_unique<AstExpr>(AstExpr::DataCast { std::move(expr), to }, cast_text + rparen_text);
	}
	if (prec <= op1_prec(Op1::Pos) && lexer.accept(Token::Op2) && lexer.get_token().get_data<Token::DataOp2>() == Op2::ArithAdd) {
		const Text op_text = lexer.step_token().get_text();
		std::unique_ptr<AstExpr> expr = parse_expr(lexer, context, op1_prec(Op1::Pos));
		const Text text = op_text + expr->text;
		return std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { Op1::Pos, op_text } }, text);
	}
	if (prec <= op1_prec(Op1::Neg) && lexer.accept(Token::Op2) && lexer.get_token().get_data<Token::DataOp2>() == Op2::ArithSub) {
		const Text op_text = lexer.step_token().get_text();
		std::unique_ptr<AstExpr> expr = parse_expr(lexer, context, op1_prec(Op1::Neg));
		const Text text = op_text + expr->text;
		return std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { Op1::Neg, op_text } }, text);
	}
	if (prec <= op1_prec(Op1::Not) && lexer.accept(Token::KwNot)) {
		const Text op_text = lexer.step_token().get_text();
		std::unique_ptr<AstExpr> expr = parse_expr(lexer, context, op1_prec(Op1::Not));
		const Text text = op_text + expr->text;
		return std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { Op1::Not, op_text } }, text);
	}
	if (lexer.accept(Token::Aggregate)) {
		if (context.inside_aggregate) {
			throw ClientError { "aggregations can not be nested", lexer.get_token().get_text() };
		}
		if (!context.accept_aggregate) {
			throw ClientError { "aggregations are invalid here", lexer.get_token().get_text() };
		}
		auto [function, function_text] = lexer.step_token().take<Token::DataAggregate>();
		lexer.expect_step(Token::LParen);
		std::unique_ptr<AstExpr> expr;
		if (lexer.accept(Token::Asterisk)) {
			const Text text_arg = lexer.step_token().get_text();
			if(function != AggregateFunction::COUNT) {
				throw ClientError { "only COUNT function accepts * as argument", text_arg };
			}
		}
		else {
			expr = parse_expr(lexer, { false, true });
		}
		const Text rparen_text = lexer.expect_step(Token::RParen).get_text();
		return std::make_unique<AstExpr>(AstExpr::DataAggregate { std::move(expr), function }, function_text + rparen_text);
	}
	lexer.unexpect();
}

static std::unique_ptr<AstExpr> parse_expr(Lexer &lexer, ExprContext context, unsigned int prec, std::unique_ptr<AstExpr> primary)
{
	ASSERT(!primary || prec == 0);
	std::unique_ptr<AstExpr> expr = primary ? std::move(primary) : parse_expr_primary(lexer, context, prec);
	for (;;) {
		if (lexer.accept(Token::Op2) || lexer.accept(Token::Asterisk)) {
			const Op2 op = lexer.get_token().get_tag() == Token::Op2 ? lexer.get_token().get_data<Token::DataOp2>() : Op2::ArithMul;
			const unsigned int prec_new = op2_prec(op);
			if (prec_new < prec) {
				break;
			}
			const Text op_text = lexer.step_token().get_text();
			std::unique_ptr<AstExpr> other = parse_expr(lexer, context, prec_new + 1);
			const Text text = expr->text + other->text;
			expr = std::make_unique<AstExpr>(AstExpr::DataOp2 { std::move(expr), std::move(other), { op, op_text } }, text);
			continue;
		}
		if (prec <= op1_prec(Op1::IsNull) && lexer.accept(Token::KwIs)) {
			const Text is_text = lexer.step_token().get_text();
			const bool negated = lexer.accept_step(Token::KwNot);
			const Text null_text = lexer.expect_step(Token::KwNull).get_text();
			const Text op_text = is_text + null_text;
			const Text text = expr->text + null_text;
			expr = std::make_unique<AstExpr>(AstExpr::DataOp1 { std::move(expr), { negated ? Op1::IsNotNull : Op1::IsNull, op_text } }, text);
			continue;
		}
		if (prec <= op1_prec(Op1::IsNull) && (lexer.accept(Token::KwNot) || lexer.accept(Token::KwBetween) || lexer.accept(Token::KwIn))) {
			const bool negated = lexer.accept_step(Token::KwNot);
			if (lexer.accept(Token::KwBetween)) {
				const Text between_text = lexer.step_token().get_text();
				std::unique_ptr<AstExpr> min = parse_expr(lexer, context, op2_prec(Op2::LogicAnd) + 1);
				const Text op_text = lexer.expect(Token::Op2).get_text();
				if (lexer.step_token().get_data<Token::DataOp2>() != Op2::LogicAnd) {
					throw ClientError { std::string { "expected " } + op2_cstr(Op2::LogicAnd), op_text };
				}
				std::unique_ptr<AstExpr> max = parse_expr(lexer, context, op2_prec(Op2::LogicAnd) + 1);
				const Text text = expr->text + max->text;
				expr = std::make_unique<AstExpr>(AstExpr::DataBetween { std::move(expr), std::move(min), std::move(max), negated, between_text }, text);
				continue;
			}
			if (lexer.accept_step(Token::KwIn)) {
				lexer.expect_step(Token::LParen);
				std::vector<std::unique_ptr<AstExpr>> list;
				do {
					list.push_back(parse_expr(lexer, context));
				} while (lexer.accept_step(Token::Comma));
				const Text rparen_text = lexer.expect_step(Token::RParen).get_text();
				const Text text = expr->text + rparen_text;
				expr = std::make_unique<AstExpr>(AstExpr::DataIn { std::move(expr), std::move(list), negated }, text);
				continue;
			}
			UNREACHABLE();
		}
		break;
	}
	return expr;
}

static AstSelectList::Expr parse_select_list_element_expr(Lexer &lexer, std::unique_ptr<AstExpr> primary = {})
{
	std::unique_ptr<AstExpr> expr = parse_expr(lexer, { true, false }, 0, std::move(primary));
	std::optional<AstIdentifier> alias;
	if (lexer.accept_step(Token::KwAs)) {
		alias = lexer.expect_step(Token::Identifier).take<Token::DataIdentifier>();
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
			[&lexer](std::unique_ptr<AstExpr> &element) -> AstSelectList::Element {
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

static std::unique_ptr<AstSource> parse_source(Lexer &lexer);

static std::unique_ptr<AstSource> parse_source_primary(Lexer &lexer)
{
	if (lexer.accept(Token::Identifier)) {
		AstIdentifier table = lexer.step_token().take<Token::DataIdentifier>();
		std::optional<AstIdentifier> alias;
		if (lexer.accept(Token::Identifier) || lexer.accept_step(Token::KwAs)) {
			alias = lexer.expect_step(Token::Identifier).take<Token::DataIdentifier>();
		}
		const Text text = alias ? table.second + alias->second : table.second;
		return std::make_unique<AstSource>(AstSource::DataTable { std::move(table), std::move(alias) }, text);
	}
	if (lexer.accept_step(Token::LParen)) {
		std::unique_ptr<AstSource> source = parse_source(lexer);
		lexer.expect_step(Token::RParen);
		return source;
	}
	lexer.unexpect();
}

static std::unique_ptr<AstSource> parse_source(Lexer &lexer)
{
	std::unique_ptr<AstSource> source_l = parse_source_primary(lexer);
	for (;;) {
		if (lexer.accept_step(Token::KwCross)) {
			lexer.expect_step(Token::KwJoin);
			std::unique_ptr<AstSource> source_r = parse_source_primary(lexer);
			const Text text = source_l->text + source_r->text;
			source_l = std::make_unique<AstSource>(AstSource::DataJoinCross { std::move(source_l), std::move(source_r) }, text);
			continue;
		}
		if (lexer.accept(Token::KwJoin) || lexer.accept(Token::KwInner) || lexer.accept(Token::KwLeft) || lexer.accept(Token::KwRight) || lexer.accept(Token::KwFull)) {
			const Text join_text = lexer.get_token().get_text();
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
			std::unique_ptr<AstSource> source_r = parse_source_primary(lexer);
			lexer.expect_step(Token::KwOn);
			std::unique_ptr<AstExpr> condition = parse_expr(lexer, { false, false });
			const Text text = join_text + condition->text;
			source_l = std::make_unique<AstSource>(AstSource::DataJoinConditional { std::move(source_l), std::move(source_r), join, std::move(condition) }, text);
			continue;
		}
		break;
	}
	return source_l;
}

static std::vector<std::unique_ptr<AstSource>> parse_sources(Lexer &lexer)
{
	lexer.expect_step(Token::KwFrom);
	std::vector<std::unique_ptr<AstSource>> sources;
	do {
		sources.push_back(parse_source(lexer));
	} while (lexer.accept_step(Token::Comma));
	return sources;
}

static std::optional<AstGroupBy> parse_group_by(Lexer &lexer)
{
	if (lexer.accept_step(Token::KwGroup)) {
		lexer.expect_step(Token::KwBy);
		std::vector<std::pair<AstExpr::DataColumn, Text>> columns;
		do {
			columns.push_back(parse_column(lexer));
		} while (lexer.accept_step(Token::Comma));
		return { { std::move(columns) } };
	}
	return std::nullopt;
}

static AstSelect parse_select(Lexer &lexer)
{
	lexer.expect_step(Token::KwSelect);
	AstSelectList list = parse_select_list(lexer);
	std::vector<std::unique_ptr<AstSource>> sources = parse_sources(lexer);
	std::unique_ptr<AstExpr> where;
	if (lexer.accept_step(Token::KwWhere)) {
		where = parse_expr(lexer, { false, false });
	}
	std::optional<AstGroupBy> group_by = parse_group_by(lexer);
	return { std::move(list), std::move(sources), std::move(where), std::move(group_by) };
}

static std::variant<AstOrderBy::Index, AstExpr::DataColumn> parse_order_by_column(Lexer &lexer)
{
	if (lexer.accept(Token::ConstantInteger)) {
		auto [index, index_text] = lexer.step_token().take<Token::DataConstantInteger>();
		ASSERT(index.value >= 0);
		return AstOrderBy::Index { { static_cast<unsigned int>(index.value), index_text } };
	}
	if (lexer.accept(Token::Identifier)) {
		auto [column, column_text_unused] = parse_column(lexer);
		return column;
	}
	lexer.unexpect();
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

AstQuery parse_query(Lexer &lexer)
{
	AstSelect select = parse_select(lexer);
	std::optional<AstOrderBy> order_by = parse_order_by(lexer);
	return { std::move(select), std::move(order_by) };
}
