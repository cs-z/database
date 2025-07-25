#include "lexer.hpp"
#include "common.hpp"

#include <algorithm>

static inline bool is_whitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline bool is_printable(char c)
{
	return 0x20 <= c && c <= 0x7E;
}

static inline bool is_alphabetic(char c)
{
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

static inline bool is_digit(char c)
{
	return '0' <= c && c <= '9';
}

static inline bool is_alphanumeric(char c)
{
	return is_alphabetic(c) || is_digit(c);
}

Lexer::Lexer(const std::string &source)
	: ptr { source.c_str() }
	, token { Token::End, ptr }
{
	step_token();
}

bool Lexer::accept(Token::Tag tag) const
{
	return token.get_tag() == tag;
}

bool Lexer::accept_step(Token::Tag tag)
{
	if (accept(tag)) {
		step_token();
		return true;
	}
	return false;
}

Token &Lexer::expect(Token::Tag tag)
{
	if (token.get_tag() != tag) {
		throw ClientError { "unexpected token. expected " + std::string { Token::tag_to_cstr(tag) }, token.get_text() };
	}
	return token;
}

Token Lexer::expect_step(Token::Tag tag)
{
	expect(tag);
	return step_token();
}

[[noreturn]] void Lexer::unexpect()
{
	throw ClientError { "unexpected token", token.get_text() };
}

Token Lexer::step_token()
{
	Token old = std::move(token);
	token = next_token();
	return old;
}

Token Lexer::next_token()
{
	while (is_whitespace(*ptr)) {
		ptr++;
	}
	const char *text_begin = ptr;
	if (*ptr == '\0') {
		return { Token::End, ptr };
	}
	if (*ptr == '(') {
		return { Token::LParen, ptr++ };
	}
	if (*ptr == ')') {
		return { Token::RParen, ptr++ };
	}
	if (*ptr == '.') {
		return { Token::Period, ptr++ };
	}
	if (*ptr == ',') {
		return { Token::Comma, ptr++ };
	}
	if (*ptr == '*') {
		return { Token::Asterisk, ptr++ };
	}
	if (*ptr == '+') {
		return { Token::Op2, Op2::ArithAdd, ptr++ };
	}
	if (*ptr == '-') {
		return { Token::Op2, Op2::ArithSub, ptr++ };
	}
	if (*ptr == '/') {
		return { Token::Op2, Op2::ArithDiv, ptr++ };
	}
	if (*ptr == '%') {
		return { Token::Op2, Op2::ArithMod, ptr++ };
	}
	if (*ptr == '=') {
		return { Token::Op2, Op2::CompEq, ptr++ };
	}
	if (*ptr == '<') {
		ptr++;
		if (*ptr == '=') {
			ptr++;
			return { Token::Op2, Op2::CompLe, { text_begin, ptr } };
		}
		if (*ptr == '>') {
			ptr++;
			return { Token::Op2, Op2::CompNe, { text_begin, ptr } };
		}
		return { Token::Op2, Op2::CompL, { text_begin, ptr } };
	}
	if (*ptr == '>') {
		ptr++;
		if (*ptr == '=') {
			ptr++;
			return { Token::Op2, Op2::CompGe, { text_begin, ptr } };
		}
		return { Token::Op2, Op2::CompG, { text_begin, ptr } };
	}
	if (is_alphabetic(*ptr)) {
		const char * const identifier_begin = ptr++;
		while (is_alphanumeric(*ptr) || *ptr == '_') {
			ptr++;
		}
		std::string identifier { identifier_begin, ptr };
		std::transform(identifier.begin(), identifier.end(), identifier.begin(), toupper);
		if (identifier == "CREATE") {
			return { Token::KwCreate, { text_begin, ptr } };
		}
		if (identifier == "TABLE") {
			return { Token::KwTable, { text_begin, ptr } };
		}
		if (identifier == "DEFAULT") {
			return { Token::KwDefault, { text_begin, ptr } };
		}
		if (identifier == "CONSTRAINT") {
			return { Token::KwConstraint, { text_begin, ptr } };
		}
		if (identifier == "NOT") {
			return { Token::KwNot, { text_begin, ptr } };
		}
		if (identifier == "UNIQUE") {
			return { Token::KwUnique, { text_begin, ptr } };
		}
		if (identifier == "PRIMARY") {
			return { Token::KwPrimary, { text_begin, ptr } };
		}
		if (identifier == "KEY") {
			return { Token::KwKey, { text_begin, ptr } };
		}
		if (identifier == "CHECK") {
			return { Token::KwCheck, { text_begin, ptr } };
		}
		if (identifier == "DROP") {
			return { Token::KwDrop, { text_begin, ptr } };
		}
		if (identifier == "INSERT") {
			return { Token::KwInsert, { text_begin, ptr } };
		}
		if (identifier == "INTO") {
			return { Token::KwInto, { text_begin, ptr } };
		}
		if (identifier == "VALUES") {
			return { Token::KwValues, { text_begin, ptr } };
		}
		if (identifier == "SELECT") {
			return { Token::KwSelect, { text_begin, ptr } };
		}
		if (identifier == "DISTINCT") {
			return { Token::KwDistinct, { text_begin, ptr } };
		}
		if (identifier == "ALL") {
			return { Token::KwAll, { text_begin, ptr } };
		}
		if (identifier == "FROM") {
			return { Token::KwFrom, { text_begin, ptr } };
		}
		if (identifier == "AS") {
			return { Token::KwAs, { text_begin, ptr } };
		}
		if (identifier == "JOIN") {
			return { Token::KwJoin, { text_begin, ptr } };
		}
		if (identifier == "CROSS") {
			return { Token::KwCross, { text_begin, ptr } };
		}
		if (identifier == "INNER") {
			return { Token::KwInner, { text_begin, ptr } };
		}
		if (identifier == "OUTER") {
			return { Token::KwOuter, { text_begin, ptr } };
		}
		if (identifier == "LEFT") {
			return { Token::KwLeft, { text_begin, ptr } };
		}
		if (identifier == "RIGHT") {
			return { Token::KwRight, { text_begin, ptr } };
		}
		if (identifier == "FULL") {
			return { Token::KwFull, { text_begin, ptr } };
		}
		if (identifier == "ON") {
			return { Token::KwOn, { text_begin, ptr } };
		}
		if (identifier == "WHERE") {
			return { Token::KwWhere, { text_begin, ptr } };
		}
		if (identifier == "GROUP") {
			return { Token::KwGroup, { text_begin, ptr } };
		}
		if (identifier == "BY") {
			return { Token::KwBy, { text_begin, ptr } };
		}
		if (identifier == "HAVING") {
			return { Token::KwHaving, { text_begin, ptr } };
		}
		if (identifier == "UNION") {
			return { Token::KwUnion, { text_begin, ptr } };
		}
		if (identifier == "EXCEPT") {
			return { Token::KwExcept, { text_begin, ptr } };
		}
		if (identifier == "INTERSECT") {
			return { Token::KwIntersect, { text_begin, ptr } };
		}
		if (identifier == "ORDER") {
			return { Token::KwOrder, { text_begin, ptr } };
		}
		if (identifier == "ASC") {
			return { Token::KwAsc, { text_begin, ptr } };
		}
		if (identifier == "DESC") {
			return { Token::KwDesc, { text_begin, ptr } };
		}
		if (identifier == "INTEGER" || identifier == "INT") {
			return { Token::KwInteger, { text_begin, ptr } };
		}
		if (identifier == "REAL" || identifier == "FLOAT" || identifier == "DOUBLE" || identifier == "DECIMAL" || identifier == "NUMERIC" || identifier == "NUMBER") {
			return { Token::KwReal, { text_begin, ptr } };
		}
		if (identifier == "VARCHAR" || identifier == "CHAR" || identifier == "TEXT") {
			return { Token::KwVarchar, { text_begin, ptr } };
		}
		if (identifier == "NULL") {
			return { Token::KwNull, { text_begin, ptr } };
		}
		if (identifier == "TRUE") {
			return { Token::ConstantBoolean, Token::DataConstantBoolean { Bool::TRUE }, { text_begin, ptr } };
		}
		if (identifier == "FALSE") {
			return { Token::ConstantBoolean, Token::DataConstantBoolean { Bool::FALSE }, { text_begin, ptr } };
		}
		if (identifier == "AND") {
			return { Token::Op2, Op2::LogicAnd, { text_begin, ptr } };
		}
		if (identifier == "OR") {
			return { Token::Op2, Op2::LogicOr, { text_begin, ptr } };
		}
		if (identifier == "COUNT") {
			return { Token::Function, Function::COUNT, { text_begin, ptr } };
		}
		if (identifier == "SUM") {
			return { Token::Function, Function::SUM, { text_begin, ptr } };
		}
		if (identifier == "AVG") {
			return { Token::Function, Function::AVG, { text_begin, ptr } };
		}
		if (identifier == "MAX") {
			return { Token::Function, Function::MAX, { text_begin, ptr } };
		}
		if (identifier == "MIN") {
			return { Token::Function, Function::MIN, { text_begin, ptr } };
		}
		if (identifier == "BETWEEN") {
			return { Token::KwBetween, { text_begin, ptr } };
		}
		if (identifier == "IN") {
			return { Token::KwIn, { text_begin, ptr } };
		}
		if (identifier == "IS") {
			return { Token::KwIs, { text_begin, ptr } };
		}
		if (identifier == "EXISTS") {
			return { Token::KwExists, { text_begin, ptr } };
		}
		if (identifier == "CAST") {
			return { Token::KwCast, { text_begin, ptr } };
		}
		return { Token::Identifier, Token::DataIdentifier { identifier }, { text_begin, ptr } };
	}
	if (is_digit(*ptr)) {
		u64 whole = 0;
		do {
			whole *= 10;
			whole += *ptr++ - '0';
		} while (is_digit(*ptr));
		if (*ptr == '.') {
			const char *fraction_begin = ++ptr;
			u64 fraction = 0;
			u64 divisor = 10;
			while (is_digit(*ptr)) {
				fraction *= 10;
				fraction += *ptr++ - '0';
				divisor *= 10;
			}
			if (ptr == fraction_begin) {
				throw ClientError { "invalid fraction", fraction_begin };
			}
			const ColumnValueReal real = static_cast<ColumnValueReal>(whole) + static_cast<ColumnValueReal>(fraction) / static_cast<ColumnValueReal>(divisor);
			return { Token::ConstantReal, Token::DataConstantReal { real }, { text_begin, ptr } };
		}
		const ColumnValueInteger integer = static_cast<ColumnValueInteger>(whole);
		return { Token::ConstantInteger, Token::DataConstantInteger { integer }, { text_begin, ptr } };
	}
	if (*ptr == '\'' || *ptr == '\"' || *ptr == '`') {
		const char delim = *ptr++;
		ColumnValueVarchar value;
		while (*ptr != '\0' && is_printable(*ptr) && *ptr != delim) {
			value.push_back(*ptr++);
		}
		if (*ptr == '\0') {
			throw ClientError { "missing string terminator", ptr };
		}
		if (!is_printable(*ptr)) {
			throw ClientError { "invalid character", ptr };
		}
		ptr++;
		return { Token::ConstantString, Token::DataConstantString { std::move(value) }, { text_begin, ptr } };
	}
	throw ClientError { "invalid character", ptr };
}
