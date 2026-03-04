#pragma once

#include "common.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

// stores location in source text, used for error reports
class SourceText
{
public:
    // TODO: move or copy

    SourceText(const SourceText& other) noexcept = default;
    SourceText(SourceText&& other) noexcept      = default;

    SourceText& operator=(const SourceText& other) noexcept = default;
    SourceText& operator=(SourceText&& other) noexcept      = default;

    explicit SourceText() : first_{}, last_{}
    {
    }

    explicit SourceText(const char* ptr) : text_{ptr, ptr + 1}, first_{ptr}, last_{ptr}
    {
    }

    explicit SourceText(std::string text, const char* begin, const char* end)
        : text_{std::move(text)}, first_{begin}, last_{end - 1}
    {
        ASSERT(begin < end);
    }

    explicit SourceText(const char* begin, const char* end)
        : text_{begin, end}, first_{begin}, last_{end - 1}
    {
        ASSERT(begin < end);
    }

    explicit SourceText(const SourceText& begin, const SourceText& end)
        : text_{begin.first_, end.first_}, first_{begin.first_}, last_{end.first_ - 1}
    {
        ASSERT(begin.first_ < end.first_);
    }

    SourceText operator+(const SourceText& other) const
    {
        ASSERT(last_ < other.first_);
        return SourceText{first_, other.last_};
    }

    [[nodiscard]] const std::string& Get() const
    {
        return text_;
    }

    void PrintEscaped() const;
    void PrintError(const std::string& source) const;

private:
    std::string text_;
    const char *first_, *last_;
};

class ClientError : public std::runtime_error
{
public:
    ClientError(const std::string& message) : std::runtime_error{message}
    {
    }
    ClientError(const std::string& message, SourceText text)
        : std::runtime_error{message}, text_{std::move(text)}
    {
    }
    void PrintError(const std::string& source) const;

private:
    std::optional<SourceText> text_;
};

class ServerError : public std::runtime_error
{
public:
    ServerError(const std::string& message) : std::runtime_error{message}
    {
    }
    ServerError(const char* function, int errnum);
    ServerError(const char* function, const std::string& arg, int errnum);
    void PrintError() const;
};
