#pragma once

#include "error.hpp"
#include "token.hpp"

#include <string>

[[nodiscard]] inline bool IsPrintable(char c)
{
    static constexpr char kMinPrintable = 0x20;
    static constexpr char kMaxPrintable = 0x7E;
    return kMinPrintable <= c && c <= kMaxPrintable;
}

[[nodiscard]] inline bool IsAlphabetic(char c)
{
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

[[nodiscard]] inline bool IsDigit(char c)
{
    return '0' <= c && c <= '9';
}

[[nodiscard]] inline bool IsAlphanumeric(char c)
{
    return IsAlphabetic(c) || IsDigit(c);
}

class Lexer
{
public:
    Lexer(const std::string& source);

    [[nodiscard]] const Token& GetToken() const
    {
        return token_;
    }

    [[nodiscard]] bool Accept(Token::Tag tag) const;
    bool               AcceptStep(Token::Tag tag);

    Token& Expect(Token::Tag tag);
    Token  ExpectStep(Token::Tag tag);

    [[noreturn]] void Unexpected();

    Token StepToken();

private:
    [[nodiscard]] Token NextToken();

    const char* ptr_;
    Token       token_;
};
