#include "parse.hpp"
#include "ast.hpp"
#include "catalog.hpp"
#include "common.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include "op.hpp"
#include "token.hpp"
#include "type.hpp"
#include "value.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

static std::pair<ColumnType, SourceText> ParseType(Lexer& lexer)
{
    if (lexer.Accept(Token::KwInteger))
    {
        const SourceText text = lexer.StepToken().GetText();
        return {ColumnType::INTEGER, text};
    }
    if (lexer.Accept(Token::KwReal))
    {
        const SourceText text = lexer.StepToken().GetText();
        return {ColumnType::REAL, text};
    }
    if (lexer.Accept(Token::KwVarchar))
    {
        const SourceText text = lexer.StepToken().GetText();
        return {ColumnType::VARCHAR, text};
    }
    lexer.Unexpected();
}

ColumnType ParseType(const std::string& name)
{
    Lexer lexer{name};
    auto [type, text_unused] = ParseType(lexer);
    lexer.Expect(Token::End);
    return type;
}

static std::pair<AstExpr::DataColumn, SourceText> ParseColumn(Lexer& lexer)
{
    SourceText name = lexer.ExpectStep(Token::Identifier).GetText();
    if (lexer.AcceptStep(Token::Period))
    {
        SourceText column = lexer.ExpectStep(Token::Identifier).GetText();
        SourceText text   = name + column;
        return std::make_pair(AstExpr::DataColumn{.table = std::move(name), .name = column},
                              std::move(column));
    }
    return std::make_pair(AstExpr::DataColumn{.table = std::nullopt, .name = name},
                          std::move(name));
}

using WildcardOrColumn =
    std::variant<AstSelectList::Wildcard, AstSelectList::TableWildcard, AstExprPtr>;
static std::optional<WildcardOrColumn> ParseWildcardOrColumn(Lexer& lexer)
{
    if (lexer.Accept(Token::Asterisk))
    {
        SourceText asterisk_text = lexer.StepToken().GetText();
        return {AstSelectList::Wildcard{std::move(asterisk_text)}};
    }
    if (lexer.Accept(Token::Identifier))
    {
        SourceText name = lexer.StepToken().GetText();
        if (lexer.AcceptStep(Token::Period))
        {
            if (lexer.Accept(Token::Asterisk))
            {
                SourceText asterisk_text = lexer.StepToken().GetText();
                return {AstSelectList::TableWildcard{.table         = std::move(name),
                                                     .asterisk_text = std::move(asterisk_text)}};
            }
            if (lexer.Accept(Token::Identifier))
            {
                SourceText column = lexer.StepToken().GetText();
                SourceText text   = name + column;
                return std::make_unique<AstExpr>(
                    AstExpr{.data = AstExpr::DataColumn{.table = std::move(name),
                                                        .name  = std::move(column)},
                            .text = std::move(text)});
            }
            lexer.Unexpected();
        }
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataColumn{.table = std::nullopt, .name = name},
                    .text = std::move(name)});
    }
    return std::nullopt;
}

struct ExprContext
{
    bool accept_aggregate;
    bool inside_aggregate;
};

static AstExprPtr ParseExpr(Lexer& lexer, ExprContext context, int prec = 0,
                            AstExprPtr primary = {});

static AstExprPtr ParseExprPrimary(Lexer& lexer, ExprContext context, int prec)
{
    if (lexer.Accept(Token::LParen))
    {
        SourceText lparen_text = lexer.StepToken().GetText();
        AstExprPtr expr        = ParseExpr(lexer, context);
        SourceText rparen_text = lexer.ExpectStep(Token::RParen).GetText();
        expr->text             = lparen_text + rparen_text;
        return expr;
    }
    if (lexer.Accept(Token::KwNull))
    {
        SourceText text = lexer.StepToken().GetText();
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataConstant{ColumnValueNull{}}, .text = std::move(text)});
    }
    if (lexer.Accept(Token::Constant))
    {
        auto [value, text] = lexer.StepToken().Take<Token::DataConstant>();
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataConstant{std::move(value)}, .text = text});
    }
    if (lexer.Accept(Token::Identifier))
    {
        auto [column, text] = ParseColumn(lexer);
        return std::make_unique<AstExpr>(AstExpr{.data = std::move(column), .text = text});
    }
    if (lexer.Accept(Token::KwCast))
    {
        SourceText cast_text = lexer.StepToken().GetText();
        lexer.ExpectStep(Token::LParen);
        AstExprPtr expr = ParseExpr(lexer, context);
        lexer.ExpectStep(Token::KwAs);
        std::pair<ColumnType, SourceText> to          = ParseType(lexer);
        SourceText                        rparen_text = lexer.ExpectStep(Token::RParen).GetText();
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataCast{.expr = std::move(expr), .to = to},
                    .text = cast_text + rparen_text});
    }
    if (prec <= Op1Prec(Op1::Pos) && lexer.Accept(Token::Op2) &&
        lexer.GetToken().GetData<Token::DataOp2>() == Op2::ArithAdd)
    {
        SourceText op_text = lexer.StepToken().GetText();
        AstExprPtr expr    = ParseExpr(lexer, context, Op1Prec(Op1::Pos));
        SourceText text    = op_text + expr->text;
        return std::make_unique<AstExpr>(AstExpr{
            .data = AstExpr::DataOp1{.expr = std::move(expr), .op = {Op1::Pos, std::move(op_text)}},
            .text = std::move(text)});
    }
    if (prec <= Op1Prec(Op1::Neg) && lexer.Accept(Token::Op2) &&
        lexer.GetToken().GetData<Token::DataOp2>() == Op2::ArithSub)
    {
        SourceText op_text = lexer.StepToken().GetText();
        AstExprPtr expr    = ParseExpr(lexer, context, Op1Prec(Op1::Neg));
        SourceText text    = op_text + expr->text;
        return std::make_unique<AstExpr>(AstExpr{
            .data = AstExpr::DataOp1{.expr = std::move(expr), .op = {Op1::Neg, std::move(op_text)}},
            .text = std::move(text)});
    }
    if (prec <= Op1Prec(Op1::Not) && lexer.Accept(Token::KwNot))
    {
        SourceText op_text = lexer.StepToken().GetText();
        AstExprPtr expr    = ParseExpr(lexer, context, Op1Prec(Op1::Not));
        SourceText text    = op_text + expr->text;
        return std::make_unique<AstExpr>(AstExpr{
            .data = AstExpr::DataOp1{.expr = std::move(expr), .op = {Op1::Not, std::move(op_text)}},
            .text = std::move(text)});
    }
    if (lexer.Accept(Token::Function))
    {
        if (context.inside_aggregate)
        {
            throw ClientError{"aggregations can not be nested", lexer.GetToken().GetText()};
        }
        if (!context.accept_aggregate)
        {
            throw ClientError{"aggregations are invalid here", lexer.GetToken().GetText()};
        }
        auto [function, function_text] = lexer.StepToken().Take<Token::DataFunction>();
        lexer.ExpectStep(Token::LParen);
        AstExprPtr arg;
        if (lexer.Accept(Token::Asterisk))
        {
            SourceText arg_text = lexer.StepToken().GetText();
            if (function != Function::COUNT)
            {
                throw ClientError{"invalid argument", std::move(arg_text)};
            }
        }
        else
        {
            arg =
                ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = true});
        }
        SourceText rparen_text = lexer.ExpectStep(Token::RParen).GetText();
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataFunction{.function = function, .arg = std::move(arg)},
                    .text = function_text + rparen_text});
    }
    lexer.Unexpected();
}

static AstExprPtr ParseExpr(Lexer& lexer, ExprContext context, int prec, AstExprPtr primary)
{
    ASSERT(!primary || prec == 0);
    AstExprPtr expr = primary ? std::move(primary) : ParseExprPrimary(lexer, context, prec);
    for (;;)
    {
        if (lexer.Accept(Token::Op2) || lexer.Accept(Token::Asterisk))
        {
            const Op2 op       = lexer.GetToken().GetTag() == Token::Op2
                                     ? lexer.GetToken().GetData<Token::DataOp2>()
                                     : Op2::ArithMul;
            const int prec_new = Op2Prec(op);
            if (prec_new < prec)
            {
                break;
            }
            SourceText op_text = lexer.StepToken().GetText();
            AstExprPtr other   = ParseExpr(lexer, context, prec_new + 1);
            SourceText text    = expr->text + other->text;
            expr               = std::make_unique<AstExpr>(
                AstExpr{.data = AstExpr::DataOp2{.expr_l = std::move(expr),
                                                               .expr_r = std::move(other),
                                                               .op     = {op, std::move(op_text)}},
                                      .text = std::move(text)});
            continue;
        }
        if (prec <= Op1Prec(Op1::IsNull) && lexer.Accept(Token::KwIs))
        {
            SourceText is_text   = lexer.StepToken().GetText();
            const bool negated   = lexer.AcceptStep(Token::KwNot);
            SourceText null_text = lexer.ExpectStep(Token::KwNull).GetText();
            SourceText op_text   = is_text + null_text;
            SourceText text      = expr->text + null_text;
            expr                 = std::make_unique<AstExpr>(
                AstExpr{.data = AstExpr::DataOp1{.expr = std::move(expr),
                                                                 .op   = {negated ? Op1::IsNotNull : Op1::IsNull,
                                                        std::move(op_text)}},
                                        .text = std::move(text)});
            continue;
        }
        if (prec <= Op1Prec(Op1::IsNull) &&
            (lexer.Accept(Token::KwNot) || lexer.Accept(Token::KwBetween) ||
             lexer.Accept(Token::KwIn)))
        {
            const bool negated = lexer.AcceptStep(Token::KwNot);
            if (lexer.Accept(Token::KwBetween))
            {
                SourceText between_text = lexer.StepToken().GetText();
                AstExprPtr min          = ParseExpr(lexer, context, Op2Prec(Op2::LogicAnd) + 1);
                if (!lexer.Accept(Token::Op2) ||
                    lexer.GetToken().GetData<Token::DataOp2>() != Op2::LogicAnd)
                {
                    throw ClientError{std::string{"expected "} + Op2Cstr(Op2::LogicAnd),
                                      lexer.GetToken().GetText()};
                }
                lexer.StepToken();
                AstExprPtr max  = ParseExpr(lexer, context, Op2Prec(Op2::LogicAnd) + 1);
                SourceText text = expr->text + max->text;
                expr            = std::make_unique<AstExpr>(AstExpr{
                    AstExpr{.data = AstExpr::DataBetween{.expr         = std::move(expr),
                                                                    .min          = std::move(min),
                                                                    .max          = std::move(max),
                                                                    .negated      = negated,
                                                                    .between_text = std::move(between_text)},
                                       .text = std::move(text)}});
                continue;
            }
            if (lexer.Accept(Token::KwIn))
            {
                SourceText in_text = lexer.StepToken().GetText();
                lexer.ExpectStep(Token::LParen);
                std::vector<AstExprPtr> list;
                do
                {
                    list.push_back(ParseExpr(lexer, context));
                } while (lexer.AcceptStep(Token::Comma));
                SourceText rparen_text = lexer.ExpectStep(Token::RParen).GetText();
                SourceText text        = expr->text + rparen_text;
                expr                   = std::make_unique<AstExpr>(
                    AstExpr{.data = AstExpr::DataIn{.expr    = std::move(expr),
                                                                      .list    = std::move(list),
                                                                      .negated = negated,
                                                                      .in_text = std::move(in_text)},
                                              .text = std::move(text)});
                continue;
            }
            UNREACHABLE();
        }
        break;
    }
    return expr;
}

static AstSelectList::Expr ParseSelectListElementExpr(Lexer& lexer, AstExprPtr primary = {})
{
    AstExprPtr expr =
        ParseExpr(lexer, ExprContext{.accept_aggregate = true, .inside_aggregate = false}, 0,
                  std::move(primary));
    std::optional<SourceText> alias;
    if (lexer.AcceptStep(Token::KwAs))
    {
        alias = lexer.ExpectStep(Token::Identifier).GetText();
    }
    return {.expr = std::move(expr), .alias = std::move(alias)};
}

static AstSelectList::Element ParseSelectListElement(Lexer& lexer)
{
    std::optional<WildcardOrColumn> result = ParseWildcardOrColumn(lexer);
    if (result)
    {
        return std::visit(
            Overload{
                [](AstSelectList::Wildcard& element) -> AstSelectList::Element { return element; },
                [](AstSelectList::TableWildcard& element) -> AstSelectList::Element
                { return element; },
                [&lexer](AstExprPtr& element) -> AstSelectList::Element
                { return ParseSelectListElementExpr(lexer, std::move(element)); },
            },
            *result);
    }
    return ParseSelectListElementExpr(lexer);
}

static AstSelectList ParseSelectList(Lexer& lexer)
{
    std::vector<AstSelectList::Element> elements;
    do
    {
        elements.push_back(ParseSelectListElement(lexer));
    } while (lexer.AcceptStep(Token::Comma));
    return {std::move(elements)};
}

static AstSourcePtr ParseSource(Lexer& lexer);

static AstSourcePtr ParseSourcePrimary(Lexer& lexer)
{
    if (lexer.Accept(Token::Identifier))
    {
        SourceText                table = lexer.StepToken().GetText();
        std::optional<SourceText> alias;
        if (lexer.Accept(Token::Identifier) || lexer.AcceptStep(Token::KwAs))
        {
            alias = lexer.ExpectStep(Token::Identifier).GetText();
        }
        SourceText text = alias ? table + *alias : table;
        return std::make_unique<AstSource>(AstSource{
            .data = AstSource::DataTable{.name = std::move(table), .alias = std::move(alias)},
            .text = std::move(text)});
    }
    if (lexer.AcceptStep(Token::LParen))
    {
        AstSourcePtr source = ParseSource(lexer);
        lexer.ExpectStep(Token::RParen);
        return source;
    }
    lexer.Unexpected();
}

static AstSourcePtr ParseSource(Lexer& lexer)
{
    AstSourcePtr source_l = ParseSourcePrimary(lexer);
    for (;;)
    {
        if (lexer.AcceptStep(Token::KwCross))
        {
            lexer.ExpectStep(Token::KwJoin);
            AstSourcePtr source_r = ParseSourcePrimary(lexer);
            SourceText   text     = source_l->text + source_r->text;
            source_l              = std::make_unique<AstSource>(
                AstSource{.data = AstSource::DataJoinCross{.source_l = std::move(source_l),
                                                                        .source_r = std::move(source_r)},
                                       .text = std::move(text)});
            continue;
        }
        if (lexer.Accept(Token::KwJoin) || lexer.Accept(Token::KwInner) ||
            lexer.Accept(Token::KwLeft) || lexer.Accept(Token::KwRight) ||
            lexer.Accept(Token::KwFull))
        {
            SourceText join_text = lexer.GetToken().GetText();
            std::optional<AstSource::DataJoinConditional::Join> join;
            if (lexer.AcceptStep(Token::KwInner))
            {
                join = AstSource::DataJoinConditional::Join::INNER;
            }
            else if (lexer.AcceptStep(Token::KwLeft))
            {
                lexer.AcceptStep(Token::KwOuter);
                join = AstSource::DataJoinConditional::Join::LEFT;
            }
            else if (lexer.AcceptStep(Token::KwRight))
            {
                lexer.AcceptStep(Token::KwOuter);
                join = AstSource::DataJoinConditional::Join::RIGHT;
            }
            else if (lexer.AcceptStep(Token::KwFull))
            {
                lexer.AcceptStep(Token::KwOuter);
                join = AstSource::DataJoinConditional::Join::FULL;
            }
            lexer.ExpectStep(Token::KwJoin);
            AstSourcePtr source_r = ParseSourcePrimary(lexer);
            lexer.ExpectStep(Token::KwOn);
            AstExprPtr condition =
                ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = false});
            SourceText text = join_text + condition->text;
            source_l        = std::make_unique<AstSource>(
                AstSource{.data = AstSource::DataJoinConditional{.source_l  = std::move(source_l),
                                                                        .source_r  = std::move(source_r),
                                                                        .join      = join,
                                                                        .condition = std::move(condition)},
                                 .text = std::move(text)});
            continue;
        }
        break;
    }
    return source_l;
}

static std::vector<AstSourcePtr> ParseSources(Lexer& lexer)
{
    lexer.ExpectStep(Token::KwFrom);
    std::vector<AstSourcePtr> sources;
    do
    {
        sources.push_back(ParseSource(lexer));
    } while (lexer.AcceptStep(Token::Comma));
    return sources;
}

static std::optional<AstGroupBy> ParseGroupBy(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::KwGroup))
    {
        lexer.ExpectStep(Token::KwBy);
        std::vector<std::pair<AstExpr::DataColumn, SourceText>> columns;
        do
        {
            columns.push_back(ParseColumn(lexer));
        } while (lexer.AcceptStep(Token::Comma));
        return {{std::move(columns)}};
    }
    return std::nullopt;
}

static AstExprPtr ParseHaving(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::KwHaving))
    {
        return ParseExpr(lexer, ExprContext{.accept_aggregate = true, .inside_aggregate = false});
    }
    return AstExprPtr{};
}

static AstSelect ParseSelect(Lexer& lexer)
{
    lexer.ExpectStep(Token::KwSelect);
    AstSelectList             list    = ParseSelectList(lexer);
    std::vector<AstSourcePtr> sources = ParseSources(lexer);
    AstExprPtr                where;
    if (lexer.AcceptStep(Token::KwWhere))
    {
        where = ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = false});
    }
    std::optional<AstGroupBy> group_by = ParseGroupBy(lexer);
    AstExprPtr                having   = ParseHaving(lexer);
    return {.list     = std::move(list),
            .sources  = std::move(sources),
            .where    = std::move(where),
            .group_by = std::move(group_by),
            .having   = std::move(having)};
}

static std::variant<AstOrderBy::Index, AstExpr::DataColumn> ParseOrderByColumn(Lexer& lexer)
{
    if (lexer.Accept(Token::Constant))
    {
        const auto* index =
            std::get_if<ColumnValueInteger>(&lexer.GetToken().GetData<Token::DataConstant>());
        if (index != nullptr)
        {
            SourceText text = lexer.StepToken().GetText();
            return AstOrderBy::Index{{static_cast<ColumnId>(*index), std::move(text)}};
        }
    }
    if (lexer.Accept(Token::Identifier))
    {
        auto [column, column_text_unused] = ParseColumn(lexer);
        return column;
    }
    lexer.Unexpected();
}

static bool ParseOrderByOrder(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::KwAsc))
    {
        return true;
    }
    return !lexer.AcceptStep(Token::KwDesc);
}

static std::optional<AstOrderBy> ParseOrderBy(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::KwOrder))
    {
        lexer.ExpectStep(Token::KwBy);
        std::vector<AstOrderBy::Column> columns;
        do
        {
            std::variant<AstOrderBy::Index, AstExpr::DataColumn> column = ParseOrderByColumn(lexer);
            const bool                                           asc    = ParseOrderByOrder(lexer);
            columns.push_back({.column = std::move(column), .asc = asc});
        } while (lexer.AcceptStep(Token::Comma));
        return {{std::move(columns)}};
    }
    return std::nullopt;
}

static std::optional<unsigned int> ParseLimit(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::KwLimit))
    {
        if (lexer.Accept(Token::Constant))
        {
            const auto* index =
                std::get_if<ColumnValueInteger>(&lexer.GetToken().GetData<Token::DataConstant>());
            if (index != nullptr)
            {
                const unsigned int limit = *index;
                lexer.StepToken();
                return limit;
            }
        }
        lexer.Unexpected();
    }
    return std::nullopt;
}

static AstQuery ParseQuery(Lexer& lexer)
{
    AstSelect                   select   = ParseSelect(lexer);
    std::optional<AstOrderBy>   order_by = ParseOrderBy(lexer);
    std::optional<unsigned int> limit    = ParseLimit(lexer);
    return {.select = std::move(select), .order_by = std::move(order_by), .limit = limit};
}

static AstCreateTable ParseCreateTable(Lexer& lexer)
{
    lexer.ExpectStep(Token::KwCreate);
    lexer.ExpectStep(Token::KwTable);
    SourceText name = lexer.ExpectStep(Token::Identifier).GetText();
    lexer.ExpectStep(Token::LParen);
    catalog::NamedColumns columns;
    do
    {
        SourceText column_name = lexer.ExpectStep(Token::Identifier).GetText();
        const auto iter =
            std::ranges::find_if(columns, [&column_name](const catalog::NamedColumn& column)
                                 { return column.first == column_name.Get(); });
        if (iter != columns.end())
        {
            throw ClientError{"column name reused", std::move(column_name)};
        }
        auto [column_type, unused] = ParseType(lexer);
        columns.emplace_back(column_name.Get(), column_type);
    } while (lexer.AcceptStep(Token::Comma) && !lexer.Accept(Token::RParen));
    lexer.ExpectStep(Token::RParen);
    return {.name = std::move(name), .columns = std::move(columns)};
}

static AstDropTable ParseDropTable(Lexer& lexer)
{
    // TODO: CASCADE | RESTRICT
    lexer.ExpectStep(Token::KwDrop);
    lexer.ExpectStep(Token::KwTable);
    SourceText name = lexer.ExpectStep(Token::Identifier).GetText();
    return {.name = std::move(name)};
}

static AstInsertValue ParseInsertValue(Lexer& lexer)
{
    lexer.ExpectStep(Token::KwInsert);
    lexer.ExpectStep(Token::KwInto);
    SourceText table = lexer.ExpectStep(Token::Identifier).GetText();
    lexer.ExpectStep(Token::KwValues);
    lexer.ExpectStep(Token::LParen);
    std::vector<AstExprPtr> exprs;
    do
    {
        exprs.push_back(
            ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = false}));
    } while (lexer.AcceptStep(Token::Comma));
    lexer.ExpectStep(Token::RParen);
    return {.table = std::move(table), .exprs = std::move(exprs)};
}

static AstUpdate ParseUpdate(Lexer& lexer)
{
    lexer.ExpectStep(Token::KwUpdate);
    auto table = lexer.ExpectStep(Token::Identifier).GetText();
    lexer.ExpectStep(Token::KwSet);

    UNREACHABLE();

    // std::vector<std::pair<SourceText, AstExprPtr>> columns;
    // do {
    //     auto name = lexer.expect_step(Token::Identifier).get_text();
    //     if (!lexer.accept(Token::Op2) || lexer.get_token().get_data<Token::DataOp2>() !=
    //     Op2::CompEq) // TODO: add to lexer class
    //     {
    //         throw ClientError{std::string{"expected "} + op2_cstr(Op2::CompEq),
    //         lexer.get_token().get_text()};
    //     }
    // } ;todo;
    //
    // AstExprPtr condition_opt;
    // if (lexer.accept_step(Token::KwWhere))
    //{
    //     condition_opt = parse_expr(lexer, ExprContext{.accept_aggregate = false,
    //     .inside_aggregate = false});
    // }

    // return {.table = std::move(table), .condition_opt = std::move(condition_opt)};
}

static AstDelete ParseDelete(Lexer& lexer)
{
    lexer.ExpectStep(Token::KwDelete);
    lexer.ExpectStep(Token::KwFrom);
    SourceText table = lexer.ExpectStep(Token::Identifier).GetText();
    AstExprPtr condition_opt;
    if (lexer.AcceptStep(Token::KwWhere))
    {
        condition_opt =
            ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = false});
    }
    return {.table = std::move(table), .condition_opt = std::move(condition_opt)};
}

AstStatement ParseStatement(Lexer& lexer)
{
    if (lexer.Accept(Token::KwCreate))
    {
        return ParseCreateTable(lexer);
    }
    if (lexer.Accept(Token::KwDrop))
    {
        return ParseDropTable(lexer);
    }
    if (lexer.Accept(Token::KwInsert))
    {
        return ParseInsertValue(lexer);
    }
    if (lexer.Accept(Token::KwSelect))
    {
        return ParseQuery(lexer);
    }
    if (lexer.Accept(Token::KwUpdate))
    {
        return ParseUpdate(lexer);
    }
    if (lexer.Accept(Token::KwDelete))
    {
        return ParseDelete(lexer);
    }
    lexer.Unexpected();
}
