#include "lexer.hpp"
#include "common.hpp"
#include "error.hpp"
#include "op.hpp"
#include "token.hpp"
#include "value.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

Lexer::Lexer(const std::string& source) : ptr_{source.c_str()}, token_{Token::kEnd, SourceText{}}
{
    StepToken();
}

bool Lexer::Accept(Token::Tag tag) const
{
    return token_.GetTag() == tag;
}

bool Lexer::AcceptStep(Token::Tag tag)
{
    if (Accept(tag))
    {
        StepToken();
        return true;
    }
    return false;
}

Token& Lexer::Expect(Token::Tag tag)
{
    if (token_.GetTag() != tag)
    {
        throw ClientError{"unexpected token. expected " + std::string{Token::TagToCstr(tag)},
                          token_.GetText()};
    }
    return token_;
}

Token Lexer::ExpectStep(Token::Tag tag)
{
    Expect(tag);
    return StepToken();
}

void Lexer::Unexpected()
{
    throw ClientError{"unexpected token", token_.GetText()};
}

Token Lexer::StepToken()
{
    Token old = std::move(token_);
    token_    = NextToken();
    return old;
}

Token Lexer::NextToken()
{
    while (std::isspace(*ptr_) != 0)
    {
        ptr_++;
    }
    if (ptr_[0] == '-' && ptr_[1] == '-')
    {
        ptr_ += 2;
        while (*ptr_ != '\0' && *ptr_ != '\r' && *ptr_ != '\n')
        {
            ptr_++;
        }
        return NextToken();
    }
    const char* text_begin = ptr_;
    if (*ptr_ == '\0')
    {
        return {Token::kEnd, SourceText{ptr_}};
    }
    if (*ptr_ == '(')
    {
        return {Token::kLParen, SourceText{ptr_++}};
    }
    if (*ptr_ == ')')
    {
        return {Token::kRParen, SourceText{ptr_++}};
    }
    if (*ptr_ == '.')
    {
        return {Token::kPeriod, SourceText{ptr_++}};
    }
    if (*ptr_ == ',')
    {
        return {Token::kComma, SourceText{ptr_++}};
    }
    if (*ptr_ == ';')
    {
        return {Token::kSemicolon, SourceText{ptr_++}};
    }
    if (*ptr_ == '*')
    {
        return {Token::kAsterisk, SourceText{ptr_++}};
    }
    if (*ptr_ == '+')
    {
        return {Token::kOp2, Op2::kArithAdd, SourceText{ptr_++}};
    }
    if (*ptr_ == '-')
    {
        return {Token::kOp2, Op2::kArithSub, SourceText{ptr_++}};
    }
    if (*ptr_ == '/')
    {
        return {Token::kOp2, Op2::kArithDiv, SourceText{ptr_++}};
    }
    if (*ptr_ == '%')
    {
        return {Token::kOp2, Op2::kArithMod, SourceText{ptr_++}};
    }
    if (*ptr_ == '=')
    {
        return {Token::kOp2, Op2::kCompEq, SourceText{ptr_++}};
    }
    if (ptr_[0] == '<' && ptr_[1] == '=')
    {
        ptr_ += 2;
        return {Token::kOp2, Op2::kCompLe, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '<' && ptr_[1] == '>')
    {
        ptr_ += 2;
        return {Token::kOp2, Op2::kCompNe, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '<')
    {
        ptr_ += 1;
        return {Token::kOp2, Op2::kCompL, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '>' && ptr_[1] == '=')
    {
        ptr_ += 2;
        return {Token::kOp2, Op2::kCompGe, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '>')
    {
        ptr_ += 1;
        return {Token::kOp2, Op2::kCompG, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '!' && ptr_[1] == '=')
    {
        ptr_ += 2;
        return {Token::kOp2, Op2::kCompNe, SourceText{text_begin, ptr_}};
    }
    if (IsAlphabetic(*ptr_))
    {
        const char* const identifier_begin = ptr_++;
        while (IsAlphanumeric(*ptr_) || *ptr_ == '_')
        {
            ptr_++;
        }
        std::string identifier{identifier_begin, ptr_};
        std::ranges::transform(identifier, identifier.begin(), toupper);
        if (identifier == "CREATE")
        {
            return {Token::kKeywordCreate, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "TABLE")
        {
            return {Token::kKeywordTable, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DEFAULT")
        {
            return {Token::kKeywordDefault, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CONSTRAINT")
        {
            return {Token::kKeywordConstraint, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "NOT")
        {
            return {Token::kKeywordNot, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "UNIQUE")
        {
            return {Token::kKeywordUnique, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "PRIMARY")
        {
            return {Token::kKeywordPrimary, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "KEY")
        {
            return {Token::kKeywordKey, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CHECK")
        {
            return {Token::kKeywordCheck, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DROP")
        {
            return {Token::kKeywordDrop, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INSERT")
        {
            return {Token::kKeywordInsert, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INTO")
        {
            return {Token::kKeywordInto, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "VALUES")
        {
            return {Token::kKeywordValues, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "SELECT")
        {
            return {Token::kKeywordSelect, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DISTINCT")
        {
            return {Token::kKeywordDistinct, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ALL")
        {
            return {Token::kKeywordAll, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "FROM")
        {
            return {Token::kKeywordFrom, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "AS")
        {
            return {Token::kKeywordAs, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "JOIN")
        {
            return {Token::kKeywordJoin, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CROSS")
        {
            return {Token::kKeywordCross, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INNER")
        {
            return {Token::kKeywordInner, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "OUTER")
        {
            return {Token::kKeywordOuter, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "LEFT")
        {
            return {Token::kKeywordLeft, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "RIGHT")
        {
            return {Token::kKeywordRight, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "FULL")
        {
            return {Token::kKeywordFull, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ON")
        {
            return {Token::kKeywordOn, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "WHERE")
        {
            return {Token::kKeywordWhere, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "GROUP")
        {
            return {Token::kKeywordGroup, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "BY")
        {
            return {Token::kKeywordBy, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "HAVING")
        {
            return {Token::kKeywordHaving, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "UNION")
        {
            return {Token::kKeywordUnion, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "EXCEPT")
        {
            return {Token::kKeywordExcept, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INTERSECT")
        {
            return {Token::kKeywordIntersect, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ORDER")
        {
            return {Token::kKeywordOrder, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ASC")
        {
            return {Token::kKeywordAsc, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DESC")
        {
            return {Token::kKeywordDesc, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INTEGER" || identifier == "INT")
        {
            return {Token::kKeywordInteger, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "REAL" || identifier == "FLOAT" || identifier == "DOUBLE" ||
            identifier == "DECIMAL" || identifier == "NUMERIC" || identifier == "NUMBER")
        {
            return {Token::kKeywordReal, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "VARCHAR" || identifier == "CHAR" || identifier == "TEXT")
        {
            return {Token::kKeywordVarchar, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "NULL")
        {
            return {Token::kKeywordNull, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "LIMIT")
        {
            return {Token::kKeywordLimit, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DELETE")
        {
            return {Token::kKeywordDelete, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "UPDATE")
        {
            return {Token::kKeywordUpdate, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "SET")
        {
            return {Token::kKeywordSet, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "TRUE")
        {
            return {Token::kConstant, Token::DataConstant{Bool::kTrue},
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "FALSE")
        {
            return {Token::kConstant, Token::DataConstant{Bool::kFalse},
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "AND")
        {
            return {Token::kOp2, Op2::kLogicAnd,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "OR")
        {
            return {Token::kOp2, Op2::kLogicOr,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "COUNT")
        {
            return {Token::kFunction, Function::kCount,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "SUM")
        {
            return {Token::kFunction, Function::kSum,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "AVG")
        {
            return {Token::kFunction, Function::kAvg,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "MAX")
        {
            return {Token::kFunction, Function::kMax,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "MIN")
        {
            return {Token::kFunction, Function::kMin,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "BETWEEN")
        {
            return {Token::kKeywordBetween, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "IN")
        {
            return {Token::kKeywordIn, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "IS")
        {
            return {Token::kKeywordIs, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "EXISTS")
        {
            return {Token::kKeywordExists, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CAST")
        {
            return {Token::kKeywordCast, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        return {Token::kIdentifier, SourceText{std::move(identifier), text_begin, ptr_}};
    }
    if (IsDigit(*ptr_))
    {
        static constexpr U64 kBase = 10;
        U64                  whole = 0;
        do
        {
            whole *= kBase;
            whole += *ptr_++ - '0';
        } while (IsDigit(*ptr_));
        if (*ptr_ == '.')
        {
            const char* fraction_begin = ++ptr_;
            U64         fraction       = 0;
            U64         divisor        = 1;
            while (IsDigit(*ptr_))
            {
                fraction *= kBase;
                fraction += *ptr_++ - '0';
                divisor *= kBase;
            }
            if (ptr_ == fraction_begin)
            {
                throw ClientError{"invalid fraction", SourceText{fraction_begin}};
            }
            const auto real =
                static_cast<ColumnValueReal>(whole) +
                (static_cast<ColumnValueReal>(fraction) / static_cast<ColumnValueReal>(divisor));
            return {Token::kConstant, Token::DataConstant{real}, SourceText{text_begin, ptr_}};
        }
        const auto integer = static_cast<ColumnValueInteger>(whole);
        return {Token::kConstant, Token::DataConstant{integer}, SourceText{text_begin, ptr_}};
    }
    if (*ptr_ == '\'' || *ptr_ == '\"' || *ptr_ == '`')
    {
        const char         delim = *ptr_++;
        ColumnValueVarchar value;
        while (*ptr_ != '\0' && IsPrintable(*ptr_) && *ptr_ != delim)
        {
            value.push_back(*ptr_++);
        }
        if (*ptr_ == '\0')
        {
            throw ClientError{"missing string terminator", SourceText{ptr_}};
        }
        if (!IsPrintable(*ptr_))
        {
            throw ClientError{"invalid character", SourceText{ptr_}};
        }
        ptr_++;
        return {Token::kConstant, Token::DataConstant{std::move(value)},
                SourceText{text_begin, ptr_}};
    }
    throw ClientError{"invalid character", SourceText{ptr_}};
}
