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
	, token { Token::End, SourceText {} }
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

[[noreturn]] void Lexer::unexpected()
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
	if (ptr[0] == '-' && ptr[1] == '-') {
		ptr += 2;
		while (*ptr != '\r' && *ptr != '\n') {
			ptr++;
		}
		return next_token();
	}
	const char *text_begin = ptr;
	if (*ptr == '\0') {
		return { Token::End, SourceText { ptr } };
	}
	if (*ptr == '(') {
		return { Token::LParen, SourceText { ptr++ } };
	}
	if (*ptr == ')') {
		return { Token::RParen, SourceText { ptr++ } };
	}
	if (*ptr == '.') {
		return { Token::Period, SourceText { ptr++ } };
	}
	if (*ptr == ',') {
		return { Token::Comma, SourceText { ptr++ } };
	}
	if (*ptr == ';') {
		return { Token::Semicolon, SourceText { ptr++ } };
	}
	if (*ptr == '*') {
		return { Token::Asterisk, SourceText { ptr++ } };
	}
	if (*ptr == '+') {
		return { Token::Op2, Op2::ArithAdd, SourceText { ptr++ } };
	}
	if (*ptr == '-') {
		return { Token::Op2, Op2::ArithSub, SourceText { ptr++ } };
	}
	if (*ptr == '/') {
		return { Token::Op2, Op2::ArithDiv, SourceText { ptr++ } };
	}
	if (*ptr == '%') {
		return { Token::Op2, Op2::ArithMod, SourceText { ptr++ } };
	}
	if (*ptr == '=') {
		return { Token::Op2, Op2::CompEq, SourceText { ptr++ } };
	}
	if (ptr[0] == '<' && ptr[1] == '=') {
		ptr += 2;
		return { Token::Op2, Op2::CompLe, SourceText { text_begin, ptr } };
	}
	if (ptr[0] == '<' && ptr[1] == '>') {
		ptr += 2;
		return { Token::Op2, Op2::CompNe, SourceText { text_begin, ptr } };
	}
	if (ptr[0] == '<') {
		ptr += 1;
		return { Token::Op2, Op2::CompL, SourceText { text_begin, ptr } };
	}
	if (ptr[0] == '>' && ptr[1] == '=') {
		ptr += 2;
		return { Token::Op2, Op2::CompGe, SourceText { text_begin, ptr } };
	}
	if (ptr[0] == '>') {
		ptr += 1;
		return { Token::Op2, Op2::CompG, SourceText { text_begin, ptr } };
	}
	if (ptr[0] == '!' && ptr[1] == '=') {
		ptr += 2;
		return { Token::Op2, Op2::CompNe, SourceText { text_begin, ptr } };
	}
	if (is_alphabetic(*ptr)) {
		const char * const identifier_begin = ptr++;
		while (is_alphanumeric(*ptr) || *ptr == '_') {
			ptr++;
		}
		std::string identifier { identifier_begin, ptr };
		std::transform(identifier.begin(), identifier.end(), identifier.begin(), toupper);
		if (identifier == "CREATE") {
			return { Token::KwCreate, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "TABLE") {
			return { Token::KwTable, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "DEFAULT") {
			return { Token::KwDefault, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "CONSTRAINT") {
			return { Token::KwConstraint, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "NOT") {
			return { Token::KwNot, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "UNIQUE") {
			return { Token::KwUnique, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "PRIMARY") {
			return { Token::KwPrimary, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "KEY") {
			return { Token::KwKey, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "CHECK") {
			return { Token::KwCheck, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "DROP") {
			return { Token::KwDrop, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "INSERT") {
			return { Token::KwInsert, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "INTO") {
			return { Token::KwInto, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "VALUES") {
			return { Token::KwValues, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "SELECT") {
			return { Token::KwSelect, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "DISTINCT") {
			return { Token::KwDistinct, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "ALL") {
			return { Token::KwAll, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "FROM") {
			return { Token::KwFrom, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "AS") {
			return { Token::KwAs, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "JOIN") {
			return { Token::KwJoin, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "CROSS") {
			return { Token::KwCross, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "INNER") {
			return { Token::KwInner, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "OUTER") {
			return { Token::KwOuter, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "LEFT") {
			return { Token::KwLeft, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "RIGHT") {
			return { Token::KwRight, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "FULL") {
			return { Token::KwFull, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "ON") {
			return { Token::KwOn, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "WHERE") {
			return { Token::KwWhere, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "GROUP") {
			return { Token::KwGroup, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "BY") {
			return { Token::KwBy, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "HAVING") {
			return { Token::KwHaving, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "UNION") {
			return { Token::KwUnion, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "EXCEPT") {
			return { Token::KwExcept, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "INTERSECT") {
			return { Token::KwIntersect, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "ORDER") {
			return { Token::KwOrder, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "ASC") {
			return { Token::KwAsc, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "DESC") {
			return { Token::KwDesc, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "INTEGER" || identifier == "INT") {
			return { Token::KwInteger, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "REAL" || identifier == "FLOAT" || identifier == "DOUBLE" || identifier == "DECIMAL" || identifier == "NUMERIC" || identifier == "NUMBER") {
			return { Token::KwReal, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "VARCHAR" || identifier == "CHAR" || identifier == "TEXT") {
			return { Token::KwVarchar, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "NULL") {
			return { Token::KwNull, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "LIMIT") {
			return { Token::KwLimit, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "TRUE") {
			return { Token::Constant, Token::DataConstant { Bool::TRUE }, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "FALSE") {
			return { Token::Constant, Token::DataConstant { Bool::FALSE }, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "AND") {
			return { Token::Op2, Op2::LogicAnd, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "OR") {
			return { Token::Op2, Op2::LogicOr, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "COUNT") {
			return { Token::Function, Function::COUNT, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "SUM") {
			return { Token::Function, Function::SUM, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "AVG") {
			return { Token::Function, Function::AVG, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "MAX") {
			return { Token::Function, Function::MAX, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "MIN") {
			return { Token::Function, Function::MIN, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "BETWEEN") {
			return { Token::KwBetween, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "IN") {
			return { Token::KwIn, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "IS") {
			return { Token::KwIs, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "EXISTS") {
			return { Token::KwExists, SourceText { std::move(identifier), text_begin, ptr } };
		}
		if (identifier == "CAST") {
			return { Token::KwCast, SourceText { std::move(identifier), text_begin, ptr } };
		}
		return { Token::Identifier, SourceText { std::move(identifier), text_begin, ptr } };
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
				throw ClientError { "invalid fraction", SourceText { fraction_begin } };
			}
			const ColumnValueReal real = static_cast<ColumnValueReal>(whole) + static_cast<ColumnValueReal>(fraction) / static_cast<ColumnValueReal>(divisor);
			return { Token::Constant, Token::DataConstant { real }, SourceText { text_begin, ptr } };
		}
		const ColumnValueInteger integer = static_cast<ColumnValueInteger>(whole);
		return { Token::Constant, Token::DataConstant { integer }, SourceText { text_begin, ptr } };
	}
	if (*ptr == '\'' || *ptr == '\"' || *ptr == '`') {
		const char delim = *ptr++;
		ColumnValueVarchar value;
		while (*ptr != '\0' && is_printable(*ptr) && *ptr != delim) {
			value.push_back(*ptr++);
		}
		if (*ptr == '\0') {
			throw ClientError { "missing string terminator", SourceText { ptr } };
		}
		if (!is_printable(*ptr)) {
			throw ClientError { "invalid character", SourceText { ptr } };
		}
		ptr++;
		return { Token::Constant, Token::DataConstant { std::move(value) }, SourceText { text_begin, ptr } };
	}
	throw ClientError { "invalid character", SourceText { ptr } };
}
