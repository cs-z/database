#include "error.hpp"

void ClientError::report(const std::string &source) const
{
	std::fprintf(stderr, "client error: %s\n", what());
	if (text) {
		ASSERT(text->first <= text->last);
		const char *padded_first = text->first;
		while (padded_first > source.c_str() && padded_first[-1] != '\n' && padded_first[-1] != '\r') {
			padded_first--;
		}
		const char *padded_last = text->last;
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
			std::fprintf(stderr, "%c", (text->first <= ptr && ptr <= text->last) ? '^' : ' ');
		}
		std::fprintf(stderr, "\n\n");
	}
}

void ServerError::report() const
{
	std::fprintf(stderr, "server error: %s\n", what());
}
