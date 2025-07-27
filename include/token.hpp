#pragma once

#include "common.hpp"
#include "op.hpp"
#include "value.hpp"
#include "error.hpp"

class Token
{
public:

	enum Tag
	{
		KwCreate,
		KwTable,
		KwDefault,
		KwConstraint,
		KwNot,
		KwUnique,
		KwPrimary,
		KwKey,
		KwCheck,
		KwDrop,
		KwInsert,
		KwInto,
		KwValues,
		KwSelect,
		KwDistinct,
		KwAll,
		KwFrom,
		KwAs,
		KwJoin,
		KwCross,
		KwInner,
		KwOuter,
		KwLeft,
		KwRight,
		KwFull,
		KwOn,
		KwWhere,
		KwGroup,
		KwBy,
		KwHaving,
		KwUnion,
		KwExcept,
		KwIntersect,
		KwOrder,
		KwAsc,
		KwDesc,
		KwInteger,
		KwReal,
		KwVarchar,
		KwBetween,
		KwIn,
		KwIs,
		KwExists,
		KwCast,
		KwNull,
		KwLimit,

		LParen,
		RParen,
		Period,
		Comma,
		Semicolon,
		Asterisk,

		Op2,
		Function,
		Identifier,

		Constant,

		End,
	};

	using DataOp2 = ::Op2;
	using DataFunction = ::Function;
	using DataConstant = ColumnValue;

	using Data = std::variant<DataOp2, DataFunction, DataConstant>;

	Token(Tag tag, SourceText text)
		: tag { tag }
		, text { std::move(text) }
	{
	}

	Token(Tag tag, Data data, SourceText text)
		: tag { tag }
		, text { std::move(text) }
		, data { std::move(data) }
	{
	}

	Token(const Token &other)
		: tag { other.tag }
		, text { other.text }
		, data { other.data }
	{
	}

	Token(Token &&other)
		: tag { other.tag }
		, text { std::move(other.text) }
		, data { std::move(other.data) }
	{
	}

	inline Token &operator=(const Token &other)
	{
		tag = other.tag;
		text = other.text;
		data = other.data;
		return *this;
	}

	inline Token &operator=(Token &&other)
	{
		tag = other.tag;
		text = std::move(other.text);
		data = std::move(other.data);
		return *this;
	}

	inline Tag get_tag() const { return tag; }
	inline SourceText get_text() const { return std::move(text); }

	template <typename DataT>
	const DataT &get_data() const
	{
		ASSERT(std::holds_alternative<DataT>(data));
		return std::get<DataT>(data);
	}

	template <typename DataT>
	std::pair<DataT, SourceText> take()
	{
		ASSERT(std::holds_alternative<DataT>(data));
		return { std::move(std::get<DataT>(data)), std::move(text) };
	}

	static const char *tag_to_cstr(Tag tag);

private:

	Tag tag;
	SourceText text;
	Data data;
};
