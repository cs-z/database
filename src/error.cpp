#include "error.hpp"

static inline char escape_char(char c)
{
	if (0x20 <= c && c <= 0x7E) {
		return c;
	}
	return ' ';
}

void SourceText::print_escaped() const
{
	ASSERT(first && last);
	bool prev_is_space = false;
	for (const char *ptr = first; ptr <= last; ptr++) {
		const char c = escape_char(*ptr);
		const bool c_is_space = std::isspace(c);
		if (!prev_is_space || !c_is_space) {
			std::printf("%c", c);
		}
		prev_is_space = c_is_space;
	}
	std::printf("\n");
}

void SourceText::print_error(const std::string &source) const
{
	ASSERT(first && last);
	const char *padded_first = first;
	while (padded_first > source.c_str() && padded_first[-1] != '\n' && padded_first[-1] != '\r') {
		padded_first--;
	}
	const char *padded_last = last;
	while (padded_last[0] != '\0' && padded_last[0] != '\r' && padded_last[0] != '\n') {
		padded_last++;
	}
	std::fprintf(stderr, "\n | ");
	for (const char *ptr = padded_first; ptr <= padded_last; ptr++) {
		std::fprintf(stderr, "%c", escape_char(*ptr));
	}
	std::fprintf(stderr, "\n | ");
	for (const char *ptr = padded_first; ptr <= padded_last; ptr++) {
		std::fprintf(stderr, "%c", (first <= ptr && ptr <= last) ? '^' : ' ');
	}
	std::fprintf(stderr, "\n\n");
}

void ClientError::print_error(const std::string &source) const
{
	std::fprintf(stderr, "client error: %s\n", what());
	if (text) {
		text->print_error(source);
	}
}

ServerError::ServerError(const char *function, int errnum)
	: std::runtime_error { "call to " + std::string { function } + "() failed: " + strerror(errnum) + " [errno = " + std::to_string(errnum) + "]" }
{
}

ServerError::ServerError(const char *function, const std::string &arg, int errnum)
	: std::runtime_error { "call to " + std::string { function } + "(" + arg + ")" + " failed: " + strerror(errnum) + " [errno = " + std::to_string(errnum) + "]" }
{
}

void ServerError::print_error() const
{
	std::fprintf(stderr, "server error: %s\n", what());
}
