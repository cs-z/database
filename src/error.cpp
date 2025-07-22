#include "error.hpp"

void Text::report(const std::string &source) const
{
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
		const char c = (0x20 <= *ptr && *ptr <= 0x7E) ? *ptr : ' ';
		std::fprintf(stderr, "%c", c);
	}
	std::fprintf(stderr, "\n | ");
	for (const char *ptr = padded_first; ptr <= padded_last; ptr++) {
		std::fprintf(stderr, "%c", (first <= ptr && ptr <= last) ? '^' : ' ');
	}
	std::fprintf(stderr, "\n\n");
}

void ClientError::report(const std::string &source) const
{
	std::fprintf(stderr, "client error: %s\n", what());
	if (text) {
		text->report(source);
	}
}

void ServerError::report() const
{
	std::fprintf(stderr, "server error: %s\n", what());
}
