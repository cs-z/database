#include "error.hpp"
#include "common.hpp"
#include "lexer.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

[[nodiscard]] static inline char EscapeChar(char c)
{
    return IsPrintable(c) ? c : ' ';
}

void SourceText::PrintEscaped() const
{
    ASSERT(first_ && last_);
    bool prev_is_space = false;
    for (const char* ptr = first_; ptr <= last_; ptr++)
    {
        const char c          = EscapeChar(*ptr);
        const bool c_is_space = std::isspace(c) != 0;
        if (!prev_is_space || !c_is_space)
        {
            std::printf("%c", c);
        }
        prev_is_space = c_is_space;
    }
    std::printf("\n");
}

void SourceText::PrintError(const std::string& source) const
{
    ASSERT(first_ && last_);
    const char* padded_first = first_;
    while (padded_first > source.c_str() && padded_first[-1] != '\n' && padded_first[-1] != '\r')
    {
        padded_first--;
    }
    const char* padded_last = last_;
    while (padded_last[0] != '\0' && padded_last[0] != '\r' && padded_last[0] != '\n')
    {
        padded_last++;
    }
    std::fprintf(stderr, "\n | ");
    for (const char* ptr = padded_first; ptr <= padded_last; ptr++)
    {
        std::fprintf(stderr, "%c", EscapeChar(*ptr));
    }
    std::fprintf(stderr, "\n | ");
    for (const char* ptr = padded_first; ptr <= padded_last; ptr++)
    {
        std::fprintf(stderr, "%c", (first_ <= ptr && ptr <= last_) ? '^' : ' ');
    }
    std::fprintf(stderr, "\n\n");
}

void ClientError::PrintError(const std::string& source) const
{
    std::fprintf(stderr, "client error: %s\n", what());
    if (text_)
    {
        text_->PrintError(source);
    }
}

ServerError::ServerError(const char* function, int errnum)
    : std::runtime_error{"call to " + std::string{function} + "() failed: " +
                         std::strerror(errnum) + " [errno = " + std::to_string(errnum) + "]"}
{
}

ServerError::ServerError(const char* function, const std::string& arg, int errnum)
    : std::runtime_error{"call to " + std::string{function} + "(" + arg + ")" + " failed: " +
                         std::strerror(errnum) + " [errno = " + std::to_string(errnum) + "]"}
{
}

void ServerError::PrintError() const
{
    std::fprintf(stderr, "server error: %s\n", what());
}
