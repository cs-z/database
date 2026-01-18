#pragma once

#include <stdexcept>

#include "common.hpp"

// stores location in source text, used for error reports
class SourceText
{
public:

	// TODO: move or copy

	SourceText(const SourceText &other) noexcept = default;
	SourceText(SourceText &&other) noexcept = default;

	SourceText &operator=(const SourceText &other) noexcept = default;
	SourceText &operator=(SourceText &&other) noexcept = default;

	explicit SourceText()
		: first {}
		, last {}
	{
	}

	explicit SourceText(const char *ptr)
		: text { ptr, ptr + 1 }
		, first { ptr }
		, last { ptr }
	{
	}

	explicit SourceText(std::string text, const char *begin, const char *end)
		: text { std::move(text) }
		, first { begin }
		, last { end - 1 }
	{
		ASSERT(begin < end);
	}

	explicit SourceText(const char *begin, const char *end)
		: text { begin, end }
		, first { begin }
		, last { end - 1 }
	{
		ASSERT(begin < end);
	}

	explicit SourceText(const SourceText &begin, const SourceText &end)
		: text { begin.first, end.first }
		, first { begin.first }
		, last { end.first - 1 }
	{
		ASSERT(begin.first < end.first);
	}

	SourceText operator+(const SourceText &other) const
	{
		ASSERT(last < other.first);
		return SourceText { first, other.last };
	}

	[[nodiscard]] const std::string &get() const { return text; }

	void print_escaped() const;
	void print_error(const std::string &source) const;

private:

	std::string text;
	const char *first, *last;
};

class ClientError : public std::runtime_error
{
public:
	ClientError(const std::string &message) : std::runtime_error { message } {}
	ClientError(const std::string &message, SourceText text) : std::runtime_error { message }, text { std::move(text) } {}
	void print_error(const std::string &source) const;
private:
	std::optional<SourceText> text;
};

class ServerError : public std::runtime_error
{
public:
	ServerError(const std::string &message) : std::runtime_error { message } {}
	ServerError(const char *function, int errnum);
	ServerError(const char *function, const std::string &arg, int errnum);
	void print_error() const;
};
