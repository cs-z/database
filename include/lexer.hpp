#pragma once

#include "token.hpp"
#include "error.hpp"

class Lexer
{
public:

	Lexer(const std::string &source);

	inline const Token &get_token() const { return token; }

	bool accept(Token::Tag tag) const;
	bool accept_step(Token::Tag tag);

	Token &expect(Token::Tag tag);
	Token expect_step(Token::Tag tag);

	[[noreturn]] void unexpected();

	Token step_token();

private:

	Token next_token();

	const char *ptr;
	Token token;
};
