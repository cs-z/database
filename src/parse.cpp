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
    if (lexer.Accept(Token::kKeywordInteger))
    {
        const SourceText text = lexer.StepToken().GetText();
        return {ColumnType::kInteger, text};
    }
    if (lexer.Accept(Token::kKeywordReal))
    {
        const SourceText text = lexer.StepToken().GetText();
        return {ColumnType::kReal, text};
    }
    if (lexer.Accept(Token::kKeywordVarchar))
    {
        const SourceText text = lexer.StepToken().GetText();
        return {ColumnType::kVarchar, text};
    }
    lexer.Unexpected();
}

ColumnType ParseType(const std::string& name)
{
    Lexer lexer{name};
    auto [type, text_unused] = ParseType(lexer);
    lexer.Expect(Token::kEnd);
    return type;
}

static std::pair<AstExpr::DataColumn, SourceText> ParseColumn(Lexer& lexer)
{
    SourceText name = lexer.ExpectStep(Token::kIdentifier).GetText();
    if (lexer.AcceptStep(Token::kPeriod))
    {
        SourceText       column = lexer.ExpectStep(Token::kIdentifier).GetText();
        const SourceText text   = name + column;
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
    if (lexer.Accept(Token::kAsterisk))
    {
        SourceText asterisk_text = lexer.StepToken().GetText();
        return {AstSelectList::Wildcard{std::move(asterisk_text)}};
    }
    if (lexer.Accept(Token::kIdentifier))
    {
        SourceText name = lexer.StepToken().GetText();
        if (lexer.AcceptStep(Token::kPeriod))
        {
            if (lexer.Accept(Token::kAsterisk))
            {
                SourceText asterisk_text = lexer.StepToken().GetText();
                return {AstSelectList::TableWildcard{.table         = std::move(name),
                                                     .asterisk_text = std::move(asterisk_text)}};
            }
            if (lexer.Accept(Token::kIdentifier))
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
    if (lexer.Accept(Token::kLParen))
    {
        const SourceText lparen_text = lexer.StepToken().GetText();
        AstExprPtr       expr        = ParseExpr(lexer, context);
        const SourceText rparen_text = lexer.ExpectStep(Token::kRParen).GetText();
        expr->text                   = lparen_text + rparen_text;
        return expr;
    }
    if (lexer.Accept(Token::kKeywordNull))
    {
        SourceText text = lexer.StepToken().GetText();
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataConstant{ColumnValueNull{}}, .text = std::move(text)});
    }
    if (lexer.Accept(Token::kConstant))
    {
        auto [value, text] = lexer.StepToken().Take<Token::DataConstant>();
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataConstant{std::move(value)}, .text = text});
    }
    if (lexer.Accept(Token::kIdentifier))
    {
        auto [column, text] = ParseColumn(lexer);
        return std::make_unique<AstExpr>(AstExpr{.data = std::move(column), .text = text});
    }
    if (lexer.Accept(Token::kKeywordCast))
    {
        const SourceText cast_text = lexer.StepToken().GetText();
        lexer.ExpectStep(Token::kLParen);
        AstExprPtr expr = ParseExpr(lexer, context);
        lexer.ExpectStep(Token::kKeywordAs);
        const std::pair<ColumnType, SourceText> to = ParseType(lexer);
        const SourceText rparen_text               = lexer.ExpectStep(Token::kRParen).GetText();
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataCast{.expr = std::move(expr), .to = to},
                    .text = cast_text + rparen_text});
    }
    if (prec <= Op1Prec(Op1::kPos) && lexer.Accept(Token::kOp2) &&
        lexer.GetToken().GetData<Token::DataOp2>() == Op2::kArithAdd)
    {
        SourceText op_text = lexer.StepToken().GetText();
        AstExprPtr expr    = ParseExpr(lexer, context, Op1Prec(Op1::kPos));
        SourceText text    = op_text + expr->text;
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataOp1{.expr = std::move(expr),
                                             .op   = {Op1::kPos, std::move(op_text)}},
                    .text = std::move(text)});
    }
    if (prec <= Op1Prec(Op1::kNeg) && lexer.Accept(Token::kOp2) &&
        lexer.GetToken().GetData<Token::DataOp2>() == Op2::kArithSub)
    {
        SourceText op_text = lexer.StepToken().GetText();
        AstExprPtr expr    = ParseExpr(lexer, context, Op1Prec(Op1::kNeg));
        SourceText text    = op_text + expr->text;
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataOp1{.expr = std::move(expr),
                                             .op   = {Op1::kNeg, std::move(op_text)}},
                    .text = std::move(text)});
    }
    if (prec <= Op1Prec(Op1::kNot) && lexer.Accept(Token::kKeywordNot))
    {
        SourceText op_text = lexer.StepToken().GetText();
        AstExprPtr expr    = ParseExpr(lexer, context, Op1Prec(Op1::kNot));
        SourceText text    = op_text + expr->text;
        return std::make_unique<AstExpr>(
            AstExpr{.data = AstExpr::DataOp1{.expr = std::move(expr),
                                             .op   = {Op1::kNot, std::move(op_text)}},
                    .text = std::move(text)});
    }
    if (lexer.Accept(Token::kFunction))
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
        lexer.ExpectStep(Token::kLParen);
        AstExprPtr arg;
        if (lexer.Accept(Token::kAsterisk))
        {
            SourceText arg_text = lexer.StepToken().GetText();
            if (function != Function::kCount)
            {
                throw ClientError{"invalid argument", std::move(arg_text)};
            }
        }
        else
        {
            arg =
                ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = true});
        }
        const SourceText rparen_text = lexer.ExpectStep(Token::kRParen).GetText();
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
        if (lexer.Accept(Token::kOp2) || lexer.Accept(Token::kAsterisk))
        {
            const Op2 op       = lexer.GetToken().GetTag() == Token::kOp2
                                     ? lexer.GetToken().GetData<Token::DataOp2>()
                                     : Op2::kArithMul;
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
        if (prec <= Op1Prec(Op1::kIsNull) && lexer.Accept(Token::kKeywordIs))
        {
            const SourceText is_text   = lexer.StepToken().GetText();
            const bool       negated   = lexer.AcceptStep(Token::kKeywordNot);
            const SourceText null_text = lexer.ExpectStep(Token::kKeywordNull).GetText();
            SourceText       op_text   = is_text + null_text;
            SourceText       text      = expr->text + null_text;
            expr                       = std::make_unique<AstExpr>(
                AstExpr{.data = AstExpr::DataOp1{.expr = std::move(expr),
                                                                       .op   = {negated ? Op1::kIsNotNull : Op1::kIsNull,
                                                        std::move(op_text)}},
                                              .text = std::move(text)});
            continue;
        }
        if (prec <= Op1Prec(Op1::kIsNull) &&
            (lexer.Accept(Token::kKeywordNot) || lexer.Accept(Token::kKeywordBetween) ||
             lexer.Accept(Token::kKeywordIn)))
        {
            const bool negated = lexer.AcceptStep(Token::kKeywordNot);
            if (lexer.Accept(Token::kKeywordBetween))
            {
                SourceText between_text = lexer.StepToken().GetText();
                AstExprPtr min          = ParseExpr(lexer, context, Op2Prec(Op2::kLogicAnd) + 1);
                if (!lexer.Accept(Token::kOp2) ||
                    lexer.GetToken().GetData<Token::DataOp2>() != Op2::kLogicAnd)
                {
                    throw ClientError{std::string{"expected "} + Op2Cstr(Op2::kLogicAnd),
                                      lexer.GetToken().GetText()};
                }
                lexer.StepToken();
                AstExprPtr max  = ParseExpr(lexer, context, Op2Prec(Op2::kLogicAnd) + 1);
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
            if (lexer.Accept(Token::kKeywordIn))
            {
                SourceText in_text = lexer.StepToken().GetText();
                lexer.ExpectStep(Token::kLParen);
                std::vector<AstExprPtr> list;
                do
                {
                    list.push_back(ParseExpr(lexer, context));
                } while (lexer.AcceptStep(Token::kComma));
                const SourceText rparen_text = lexer.ExpectStep(Token::kRParen).GetText();
                SourceText       text        = expr->text + rparen_text;
                expr                         = std::make_unique<AstExpr>(
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
    if (lexer.AcceptStep(Token::kKeywordAs))
    {
        alias = lexer.ExpectStep(Token::kIdentifier).GetText();
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
    } while (lexer.AcceptStep(Token::kComma));
    return {std::move(elements)};
}

static AstSourcePtr ParseSource(Lexer& lexer);

static AstSourcePtr ParseSourcePrimary(Lexer& lexer)
{
    if (lexer.Accept(Token::kIdentifier))
    {
        SourceText                table = lexer.StepToken().GetText();
        std::optional<SourceText> alias;
        if (lexer.Accept(Token::kIdentifier) || lexer.AcceptStep(Token::kKeywordAs))
        {
            alias = lexer.ExpectStep(Token::kIdentifier).GetText();
        }
        SourceText text = alias ? table + *alias : table;
        return std::make_unique<AstSource>(AstSource{
            .data = AstSource::DataTable{.name = std::move(table), .alias = std::move(alias)},
            .text = std::move(text)});
    }
    if (lexer.AcceptStep(Token::kLParen))
    {
        AstSourcePtr source = ParseSource(lexer);
        lexer.ExpectStep(Token::kRParen);
        return source;
    }
    lexer.Unexpected();
}

static AstSourcePtr ParseSource(Lexer& lexer)
{
    AstSourcePtr source_l = ParseSourcePrimary(lexer);
    for (;;)
    {
        if (lexer.AcceptStep(Token::kKeywordCross))
        {
            lexer.ExpectStep(Token::kKeywordJoin);
            AstSourcePtr source_r = ParseSourcePrimary(lexer);
            SourceText   text     = source_l->text + source_r->text;
            source_l              = std::make_unique<AstSource>(
                AstSource{.data = AstSource::DataJoinCross{.source_l = std::move(source_l),
                                                                        .source_r = std::move(source_r)},
                                       .text = std::move(text)});
            continue;
        }
        if (lexer.Accept(Token::kKeywordJoin) || lexer.Accept(Token::kKeywordInner) ||
            lexer.Accept(Token::kKeywordLeft) || lexer.Accept(Token::kKeywordRight) ||
            lexer.Accept(Token::kKeywordFull))
        {
            const SourceText join_text = lexer.GetToken().GetText();
            std::optional<AstSource::DataJoinConditional::Join> join;
            if (lexer.AcceptStep(Token::kKeywordInner))
            {
                join = AstSource::DataJoinConditional::Join::kInner;
            }
            else if (lexer.AcceptStep(Token::kKeywordLeft))
            {
                lexer.AcceptStep(Token::kKeywordOuter);
                join = AstSource::DataJoinConditional::Join::kLeft;
            }
            else if (lexer.AcceptStep(Token::kKeywordRight))
            {
                lexer.AcceptStep(Token::kKeywordOuter);
                join = AstSource::DataJoinConditional::Join::kRight;
            }
            else if (lexer.AcceptStep(Token::kKeywordFull))
            {
                lexer.AcceptStep(Token::kKeywordOuter);
                join = AstSource::DataJoinConditional::Join::kFull;
            }
            lexer.ExpectStep(Token::kKeywordJoin);
            AstSourcePtr source_r = ParseSourcePrimary(lexer);
            lexer.ExpectStep(Token::kKeywordOn);
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
    lexer.ExpectStep(Token::kKeywordFrom);
    std::vector<AstSourcePtr> sources;
    do
    {
        sources.push_back(ParseSource(lexer));
    } while (lexer.AcceptStep(Token::kComma));
    return sources;
}

static std::optional<AstGroupBy> ParseGroupBy(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::kKeywordGroup))
    {
        lexer.ExpectStep(Token::kKeywordBy);
        std::vector<std::pair<AstExpr::DataColumn, SourceText>> columns;
        do
        {
            columns.push_back(ParseColumn(lexer));
        } while (lexer.AcceptStep(Token::kComma));
        return {{std::move(columns)}};
    }
    return std::nullopt;
}

static AstExprPtr ParseHaving(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::kKeywordHaving))
    {
        return ParseExpr(lexer, ExprContext{.accept_aggregate = true, .inside_aggregate = false});
    }
    return AstExprPtr{};
}

static AstSelect ParseSelect(Lexer& lexer)
{
    lexer.ExpectStep(Token::kKeywordSelect);
    AstSelectList             list    = ParseSelectList(lexer);
    std::vector<AstSourcePtr> sources = ParseSources(lexer);
    AstExprPtr                where;
    if (lexer.AcceptStep(Token::kKeywordWhere))
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
    if (lexer.Accept(Token::kConstant))
    {
        const auto* index =
            std::get_if<ColumnValueInteger>(&lexer.GetToken().GetData<Token::DataConstant>());
        if (index != nullptr)
        {
            SourceText text = lexer.StepToken().GetText();
            return AstOrderBy::Index{{static_cast<ColumnId>(*index), std::move(text)}};
        }
    }
    if (lexer.Accept(Token::kIdentifier))
    {
        auto [column, column_text_unused] = ParseColumn(lexer);
        return column;
    }
    lexer.Unexpected();
}

static bool ParseOrderByOrder(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::kKeywordAsc))
    {
        return true;
    }
    return !lexer.AcceptStep(Token::kKeywordDesc);
}

static std::optional<AstOrderBy> ParseOrderBy(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::kKeywordOrder))
    {
        lexer.ExpectStep(Token::kKeywordBy);
        std::vector<AstOrderBy::Column> columns;
        do
        {
            std::variant<AstOrderBy::Index, AstExpr::DataColumn> column = ParseOrderByColumn(lexer);
            const bool                                           asc    = ParseOrderByOrder(lexer);
            columns.push_back({.column = std::move(column), .asc = asc});
        } while (lexer.AcceptStep(Token::kComma));
        return {{std::move(columns)}};
    }
    return std::nullopt;
}

static std::optional<unsigned int> ParseLimit(Lexer& lexer)
{
    if (lexer.AcceptStep(Token::kKeywordLimit))
    {
        if (lexer.Accept(Token::kConstant))
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
    AstSelect                         select   = ParseSelect(lexer);
    std::optional<AstOrderBy>         order_by = ParseOrderBy(lexer);
    const std::optional<unsigned int> limit    = ParseLimit(lexer);
    return {.select = std::move(select), .order_by = std::move(order_by), .limit = limit};
}

static AstCreateTable ParseCreateTable(Lexer& lexer)
{
    lexer.ExpectStep(Token::kKeywordCreate);
    lexer.ExpectStep(Token::kKeywordTable);
    SourceText name = lexer.ExpectStep(Token::kIdentifier).GetText();
    lexer.ExpectStep(Token::kLParen);
    catalog::NamedColumns columns;
    do
    {
        SourceText column_name = lexer.ExpectStep(Token::kIdentifier).GetText();
        const auto iter =
            std::ranges::find_if(columns, [&column_name](const catalog::NamedColumn& column)
                                 { return column.first == column_name.Get(); });
        if (iter != columns.end())
        {
            throw ClientError{"column name reused", std::move(column_name)};
        }
        auto [column_type, unused] = ParseType(lexer);
        columns.emplace_back(column_name.Get(), column_type);
    } while (lexer.AcceptStep(Token::kComma) && !lexer.Accept(Token::kRParen));
    lexer.ExpectStep(Token::kRParen);
    return {.name = std::move(name), .columns = std::move(columns)};
}

static AstDropTable ParseDropTable(Lexer& lexer)
{
    // TODO: CASCADE | RESTRICT
    lexer.ExpectStep(Token::kKeywordDrop);
    lexer.ExpectStep(Token::kKeywordTable);
    SourceText name = lexer.ExpectStep(Token::kIdentifier).GetText();
    return {.name = std::move(name)};
}

static AstInsertValue ParseInsertValue(Lexer& lexer)
{
    lexer.ExpectStep(Token::kKeywordInsert);
    lexer.ExpectStep(Token::kKeywordInto);
    SourceText table = lexer.ExpectStep(Token::kIdentifier).GetText();
    lexer.ExpectStep(Token::kKeywordValues);
    lexer.ExpectStep(Token::kLParen);
    std::vector<AstExprPtr> exprs;
    do
    {
        exprs.push_back(
            ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = false}));
    } while (lexer.AcceptStep(Token::kComma));
    lexer.ExpectStep(Token::kRParen);
    return {.table = std::move(table), .exprs = std::move(exprs)};
}

static AstUpdate ParseUpdate(Lexer& lexer)
{
    lexer.ExpectStep(Token::kKeywordUpdate);
    auto table = lexer.ExpectStep(Token::kIdentifier).GetText();
    lexer.ExpectStep(Token::kKeywordSet);

    UNREACHABLE();

    // std::vector<std::pair<SourceText, AstExprPtr>> columns;
    // do {
    //     auto name = lexer.expect_step(Token::Identifier).get_text();
    //     if (!lexer.accept(Token::Op2) || lexer.get_token().get_data<Token::DataOp2>() !=
    //     Op2::kCompEq) // TODO: add to lexer class
    //     {
    //         throw ClientError{std::string{"expected "} + op2_cstr(Op2::kCompEq),
    //         lexer.get_token().get_text()};
    //     }
    // } ;todo;
    //
    // AstExprPtr condition_opt;
    // if (lexer.accept_step(Token::kKeywordWhere))
    //{
    //     condition_opt = parse_expr(lexer, ExprContext{.accept_aggregate = false,
    //     .inside_aggregate = false});
    // }

    // return {.table = std::move(table), .condition_opt = std::move(condition_opt)};
}

static AstDelete ParseDelete(Lexer& lexer)
{
    lexer.ExpectStep(Token::kKeywordDelete);
    lexer.ExpectStep(Token::kKeywordFrom);
    SourceText table = lexer.ExpectStep(Token::kIdentifier).GetText();
    AstExprPtr condition_opt;
    if (lexer.AcceptStep(Token::kKeywordWhere))
    {
        condition_opt =
            ParseExpr(lexer, ExprContext{.accept_aggregate = false, .inside_aggregate = false});
    }
    return {.table = std::move(table), .condition_opt = std::move(condition_opt)};
}

AstStatement ParseStatement(Lexer& lexer)
{
    if (lexer.Accept(Token::kKeywordCreate))
    {
        return ParseCreateTable(lexer);
    }
    if (lexer.Accept(Token::kKeywordDrop))
    {
        return ParseDropTable(lexer);
    }
    if (lexer.Accept(Token::kKeywordInsert))
    {
        return ParseInsertValue(lexer);
    }
    if (lexer.Accept(Token::kKeywordSelect))
    {
        return ParseQuery(lexer);
    }
    if (lexer.Accept(Token::kKeywordUpdate))
    {
        return ParseUpdate(lexer);
    }
    if (lexer.Accept(Token::kKeywordDelete))
    {
        return ParseDelete(lexer);
    }
    lexer.Unexpected();
}
