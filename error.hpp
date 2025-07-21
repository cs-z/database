#pragma once

#include <stdexcept>

#include "common.hpp"

// stores location in source text
// used in error reports
struct Text
{
	Text(const char *ptr) : first { ptr }, last { ptr } {}
	Text(const char *begin, const char *end)
	{
		ASSERT(begin < end);
		first = begin;
		last = end - 1;
	}
	[[nodiscard]] Text operator+(const Text &other) const
	{
		ASSERT(last < other.first);
		return { first, other.last };
	}
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
	void report() const;
};
