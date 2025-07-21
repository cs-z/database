#pragma once

#include "common.hpp"
#include "op.hpp"
#include "value.hpp"
#include "aggregate.hpp"
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

		LParen,
		RParen,
		Period,
		Comma,
		Asterisk,

		Op2,
		Aggregate,
		Identifier,

		ConstantBoolean,
		ConstantInteger,
		ConstantReal,
		ConstantString,

		End,
	};

	using DataOp2 = ::Op2;
	using DataAggregate = AggregateFunction;
	using DataIdentifier = std::string;
	// structs for avoiding type collision in variant
	struct DataConstantBoolean { ColumnValueBoolean value; };
	struct DataConstantInteger { ColumnValueInteger value; };
	struct DataConstantReal { ColumnValueReal value; };
	struct DataConstantString { ColumnValueVarchar value; };

	using Data = std::variant<DataOp2, DataAggregate, DataIdentifier, DataConstantBoolean, DataConstantInteger, DataConstantReal, DataConstantString>;

	Token(Tag tag, Text text) : tag { tag }, text { text } {}
	Token(Tag tag, Data data, Text text) : tag { tag }, data { std::move(data) }, text { text } {}

	Token(const Token &other) : tag { other.tag }, data { other.data }, text { other.text } {}
	Token(Token &&other) : tag { other.tag }, data { std::move(other.data) }, text { other.text } {}

	inline Token &operator=(const Token &other)
	{
		tag = other.tag;
		data = other.data;
		text = other.text;
		return *this;
	}

	inline Token &operator=(Token &&other)
	{
		tag = other.tag;
		data = std::move(other.data);
		text = other.text;
		return *this;
	}

	inline Tag get_tag() const { return tag; }
	inline Text get_text() const { return text; }

	template <typename DataT>
	std::pair<DataT, Text> take()
	{
		ASSERT(std::holds_alternative<DataT>(data));
		return { std::move(std::get<DataT>(data)), text };
	}

	template <typename DataT>
	const DataT &get_data() const
	{
		ASSERT(std::holds_alternative<DataT>(data));
		return std::get<DataT>(data);
	}

	static const char *tag_to_cstr(Tag tag);

private:

	Tag tag;
	Data data;
	Text text;
};
