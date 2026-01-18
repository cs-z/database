#pragma once

#include "error.hpp"
#include "token.hpp"

inline bool is_printable(char c)
{
    static constexpr char minPrintable = 0x20;
    static constexpr char maxPrintable = 0x7E;
    return minPrintable <= c && c <= maxPrintable;
}

inline bool is_alphabetic(char c)
{
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

inline bool is_digit(char c)
{
    return '0' <= c && c <= '9';
}

inline bool is_alphanumeric(char c)
{
    return is_alphabetic(c) || is_digit(c);
}

class Lexer
{
  public:
    Lexer(const std::string& source);

    [[nodiscard]] inline const Token& get_token() const { return token; }

    [[nodiscard]] bool accept(Token::Tag tag) const;
    bool               accept_step(Token::Tag tag);

    Token& expect(Token::Tag tag);
    Token  expect_step(Token::Tag tag);

    [[noreturn]] void unexpected();

    Token step_token();

  private:
    [[nodiscard]] Token next_token();

    const char* ptr;
    Token       token;
};
