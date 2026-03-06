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
        kKeywordCreate,
        kKeywordTable,
        kKeywordDefault,
        kKeywordConstraint,
        kKeywordNot,
        kKeywordUnique,
        kKeywordPrimary,
        kKeywordKey,
        kKeywordCheck,
        kKeywordDrop,
        kKeywordInsert,
        kKeywordInto,
        kKeywordValues,
        kKeywordSelect,
        kKeywordDistinct,
        kKeywordAll,
        kKeywordFrom,
        kKeywordAs,
        kKeywordJoin,
        kKeywordCross,
        kKeywordInner,
        kKeywordOuter,
        kKeywordLeft,
        kKeywordRight,
        kKeywordFull,
        kKeywordOn,
        kKeywordWhere,
        kKeywordGroup,
        kKeywordBy,
        kKeywordHaving,
        kKeywordUnion,
        kKeywordExcept,
        kKeywordIntersect,
        kKeywordOrder,
        kKeywordAsc,
        kKeywordDesc,
        kKeywordInteger,
        kKeywordReal,
        kKeywordVarchar,
        kKeywordBetween,
        kKeywordIn,
        kKeywordIs,
        kKeywordExists,
        kKeywordCast,
        kKeywordNull,
        kKeywordLimit,
        kKeywordDelete,
        kKeywordUpdate,
        kKeywordSet,

        kLParen,
        kRParen,
        kPeriod,
        kComma,
        kSemicolon,
        kAsterisk,

        kOp2,
        kFunction,
        kIdentifier,

        kConstant,

        kEnd,
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
    ~Token()                                      = default;

    [[nodiscard]] Tag GetTag() const
    {
        return tag_;
    }
    [[nodiscard]] const SourceText& GetText() const
    {
        return text_;
    } // TODO: maybe move

    template <typename T> [[nodiscard]] const T& GetData() const
    {
        ASSERT(std::holds_alternative<T>(data_));
        return std::get<T>(data_);
    }

    template <typename T> [[nodiscard]] std::pair<T, SourceText> Take()
    {
        ASSERT(std::holds_alternative<T>(data_));
        return {std::move(std::get<T>(data_)), std::move(text_)};
    }

    [[nodiscard]] static const char* TagToCstr(Tag tag);

private:
    Tag        tag_;
    SourceText text_;
    Data       data_;
};
