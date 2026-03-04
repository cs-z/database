#pragma once

#include "common.hpp"
#include "error.hpp"
#include "op.hpp"
#include "value.hpp"

#include <cstdint>
#include <memory>
#include <utility>

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
        KwDelete,
        KwUpdate,
        KwSet,

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

    Token(Tag tag, SourceText text) : tag_{tag}, text_{std::move(text)}
    {
    }

    Token(Tag tag, Data data, SourceText text)
        : tag_{tag}, text_{std::move(text)}, data_{std::move(data)}
    {
    }

    Token(const Token& other) noexcept            = default;
    Token(Token&& other) noexcept                 = default;
    Token& operator=(const Token& other) noexcept = default;
    Token& operator=(Token&& other) noexcept      = default;

    [[nodiscard]] Tag GetTag() const
    {
        return tag_;
    }
    [[nodiscard]] const SourceText& GetText() const
    {
        return text_;
    } // TODO: maybe move

    template <typename DataT> [[nodiscard]] const DataT& GetData() const
    {
        ASSERT(std::holds_alternative<DataT>(data_));
        return std::get<DataT>(data_);
    }

    template <typename DataT> [[nodiscard]] std::pair<DataT, SourceText> Take()
    {
        ASSERT(std::holds_alternative<DataT>(data_));
        return {std::move(std::get<DataT>(data_)), std::move(text_)};
    }

    [[nodiscard]] static const char* TagToCstr(Tag tag);

private:
    Tag        tag_;
    SourceText text_;
    Data       data_;
};
