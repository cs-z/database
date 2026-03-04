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

Lexer::Lexer(const std::string& source) : ptr_{source.c_str()}, token_{Token::End, SourceText{}}
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
        return {Token::End, SourceText{ptr_}};
    }
    if (*ptr_ == '(')
    {
        return {Token::LParen, SourceText{ptr_++}};
    }
    if (*ptr_ == ')')
    {
        return {Token::RParen, SourceText{ptr_++}};
    }
    if (*ptr_ == '.')
    {
        return {Token::Period, SourceText{ptr_++}};
    }
    if (*ptr_ == ',')
    {
        return {Token::Comma, SourceText{ptr_++}};
    }
    if (*ptr_ == ';')
    {
        return {Token::Semicolon, SourceText{ptr_++}};
    }
    if (*ptr_ == '*')
    {
        return {Token::Asterisk, SourceText{ptr_++}};
    }
    if (*ptr_ == '+')
    {
        return {Token::Op2, Op2::ArithAdd, SourceText{ptr_++}};
    }
    if (*ptr_ == '-')
    {
        return {Token::Op2, Op2::ArithSub, SourceText{ptr_++}};
    }
    if (*ptr_ == '/')
    {
        return {Token::Op2, Op2::ArithDiv, SourceText{ptr_++}};
    }
    if (*ptr_ == '%')
    {
        return {Token::Op2, Op2::ArithMod, SourceText{ptr_++}};
    }
    if (*ptr_ == '=')
    {
        return {Token::Op2, Op2::CompEq, SourceText{ptr_++}};
    }
    if (ptr_[0] == '<' && ptr_[1] == '=')
    {
        ptr_ += 2;
        return {Token::Op2, Op2::CompLe, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '<' && ptr_[1] == '>')
    {
        ptr_ += 2;
        return {Token::Op2, Op2::CompNe, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '<')
    {
        ptr_ += 1;
        return {Token::Op2, Op2::CompL, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '>' && ptr_[1] == '=')
    {
        ptr_ += 2;
        return {Token::Op2, Op2::CompGe, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '>')
    {
        ptr_ += 1;
        return {Token::Op2, Op2::CompG, SourceText{text_begin, ptr_}};
    }
    if (ptr_[0] == '!' && ptr_[1] == '=')
    {
        ptr_ += 2;
        return {Token::Op2, Op2::CompNe, SourceText{text_begin, ptr_}};
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
            return {Token::KwCreate, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "TABLE")
        {
            return {Token::KwTable, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DEFAULT")
        {
            return {Token::KwDefault, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CONSTRAINT")
        {
            return {Token::KwConstraint, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "NOT")
        {
            return {Token::KwNot, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "UNIQUE")
        {
            return {Token::KwUnique, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "PRIMARY")
        {
            return {Token::KwPrimary, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "KEY")
        {
            return {Token::KwKey, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CHECK")
        {
            return {Token::KwCheck, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DROP")
        {
            return {Token::KwDrop, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INSERT")
        {
            return {Token::KwInsert, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INTO")
        {
            return {Token::KwInto, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "VALUES")
        {
            return {Token::KwValues, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "SELECT")
        {
            return {Token::KwSelect, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DISTINCT")
        {
            return {Token::KwDistinct, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ALL")
        {
            return {Token::KwAll, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "FROM")
        {
            return {Token::KwFrom, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "AS")
        {
            return {Token::KwAs, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "JOIN")
        {
            return {Token::KwJoin, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CROSS")
        {
            return {Token::KwCross, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INNER")
        {
            return {Token::KwInner, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "OUTER")
        {
            return {Token::KwOuter, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "LEFT")
        {
            return {Token::KwLeft, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "RIGHT")
        {
            return {Token::KwRight, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "FULL")
        {
            return {Token::KwFull, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ON")
        {
            return {Token::KwOn, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "WHERE")
        {
            return {Token::KwWhere, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "GROUP")
        {
            return {Token::KwGroup, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "BY")
        {
            return {Token::KwBy, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "HAVING")
        {
            return {Token::KwHaving, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "UNION")
        {
            return {Token::KwUnion, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "EXCEPT")
        {
            return {Token::KwExcept, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INTERSECT")
        {
            return {Token::KwIntersect, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ORDER")
        {
            return {Token::KwOrder, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "ASC")
        {
            return {Token::KwAsc, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DESC")
        {
            return {Token::KwDesc, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "INTEGER" || identifier == "INT")
        {
            return {Token::KwInteger, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "REAL" || identifier == "FLOAT" || identifier == "DOUBLE" ||
            identifier == "DECIMAL" || identifier == "NUMERIC" || identifier == "NUMBER")
        {
            return {Token::KwReal, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "VARCHAR" || identifier == "CHAR" || identifier == "TEXT")
        {
            return {Token::KwVarchar, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "NULL")
        {
            return {Token::KwNull, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "LIMIT")
        {
            return {Token::KwLimit, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "DELETE")
        {
            return {Token::KwDelete, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "UPDATE")
        {
            return {Token::KwUpdate, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "SET")
        {
            return {Token::KwSet, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "TRUE")
        {
            return {Token::Constant, Token::DataConstant{Bool::TRUE},
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "FALSE")
        {
            return {Token::Constant, Token::DataConstant{Bool::FALSE},
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "AND")
        {
            return {Token::Op2, Op2::LogicAnd, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "OR")
        {
            return {Token::Op2, Op2::LogicOr, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "COUNT")
        {
            return {Token::Function, Function::COUNT,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "SUM")
        {
            return {Token::Function, Function::SUM,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "AVG")
        {
            return {Token::Function, Function::AVG,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "MAX")
        {
            return {Token::Function, Function::MAX,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "MIN")
        {
            return {Token::Function, Function::MIN,
                    SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "BETWEEN")
        {
            return {Token::KwBetween, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "IN")
        {
            return {Token::KwIn, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "IS")
        {
            return {Token::KwIs, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "EXISTS")
        {
            return {Token::KwExists, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        if (identifier == "CAST")
        {
            return {Token::KwCast, SourceText{std::move(identifier), text_begin, ptr_}};
        }
        return {Token::Identifier, SourceText{std::move(identifier), text_begin, ptr_}};
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
            return {Token::Constant, Token::DataConstant{real}, SourceText{text_begin, ptr_}};
        }
        const auto integer = static_cast<ColumnValueInteger>(whole);
        return {Token::Constant, Token::DataConstant{integer}, SourceText{text_begin, ptr_}};
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
        return {Token::Constant, Token::DataConstant{std::move(value)},
                SourceText{text_begin, ptr_}};
    }
    throw ClientError{"invalid character", SourceText{ptr_}};
}
