#pragma once

#include <stdexcept>

#include "common.hpp"

// stores location in source text, used for error reports
class SourceText
{
public:

	explicit SourceText()
		: text {}
		, first {}
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

	SourceText operator+(const SourceText &other) const
	{
		ASSERT(last < other.first);
		return SourceText { first, other.last };
	}

	inline const std::string &get() const { return text; }

	void report(const std::string &source) const;

private:

	std::string text;
	const char *first, *last;
};

class ClientError : public std::runtime_error
{
public:
	ClientError(std::string message) : std::runtime_error { message } {}
	ClientError(std::string message, SourceText text) : std::runtime_error { message }, text { text } {}
	void report(const std::string &source) const;
private:
	std::optional<SourceText> text;
};

class ServerError : public std::runtime_error
{
public:
	ServerError(std::string message) : std::runtime_error { message } {}
	ServerError(const char *function, int errnum);
	ServerError(const char *function, const std::string &arg, int errnum);
	void report() const;
};
