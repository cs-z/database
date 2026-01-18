#include "common.hpp"

void abort_expr(const char* expr, const char* file, long line)
{
    std::fprintf(stderr, "ABORT: test failed at %s:%ld by expression \"%s\"\n", file, line, expr);
    // std::exit(EXIT_FAILURE);
    abort();  // TODO
}
