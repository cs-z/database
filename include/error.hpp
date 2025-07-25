#pragma once

#include <stdexcept>

#include "common.hpp"

// stores location in source text
// used in error reports
class Text
{
public:

	Text(const char *ptr) : first { ptr }, last { ptr }
	{
		ASSERT(ptr);
	}

	Text(const char *begin, const char *end) : first { begin }, last { end - 1 }
	{
		ASSERT(first && last && first <= last);
	}

	Text(const Text &other) : first { other.first }, last { other.last } {}

	Text &operator=(const Text &other)
	{
		ASSERT(other.first && other.last && other.first <= other.last);
		first = other.first;
		last = other.last;
		return *this;
	}

	Text operator+(const Text &other) const
	{
		ASSERT(last < other.first);
		return { first, other.last };
	}

	void report(const std::string &source) const;

private:

	const char *first, *last;
};

class ClientError : public std::runtime_error
{
public:
	ClientError(std::string message) : std::runtime_error { message } {}
	ClientError(std::string message, Text text) : std::runtime_error { message }, text { text } {}
	void report(const std::string &source) const;
private:
	std::optional<Text> text;
};

class ServerError : public std::runtime_error
{
public:
	ServerError(std::string message) : std::runtime_error { message } {}
	ServerError(const char *function, int errnum);
	ServerError(const char *function, const std::string &arg, int errnum);
	void report() const;
};
