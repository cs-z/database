#pragma once

#include "common.hpp"
#include "error.hpp"
#include "op.hpp"
#include "value.hpp"
#include <cstdint>

class Token
{
  public:
    enum Tag : std::uint8_t
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

    using DataOp2      = ::Op2;
    using DataFunction = ::Function;
    using DataConstant = ColumnValue;

    using Data = std::variant<DataOp2, DataFunction, DataConstant>;

    Token(Tag tag, SourceText text) : tag{tag}, text{std::move(text)} {}

    Token(Tag tag, Data data, SourceText text)
        : tag{tag}, text{std::move(text)}, data{std::move(data)}
    {
    }

    Token(const Token& other) noexcept            = default;
    Token(Token&& other) noexcept                 = default;
    Token& operator=(const Token& other) noexcept = default;
    Token& operator=(Token&& other) noexcept      = default;

    [[nodiscard]] Tag               get_tag() const { return tag; }
    [[nodiscard]] const SourceText& get_text() const { return text; }  // TODO: maybe move

    template <typename DataT> [[nodiscard]] const DataT& get_data() const
    {
        ASSERT(std::holds_alternative<DataT>(data));
        return std::get<DataT>(data);
    }

    template <typename DataT> [[nodiscard]] std::pair<DataT, SourceText> take()
    {
        ASSERT(std::holds_alternative<DataT>(data));
        return {std::move(std::get<DataT>(data)), std::move(text)};
    }

    static const char* tag_to_cstr(Tag tag);

  private:
    Tag        tag;
    SourceText text;
    Data       data;
};
