#include "common.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

void AbortExpr(const char* expr, const char* file, std::int64_t line)
{
    std::fprintf(stderr, "ABORT: test failed at %s:%ld by expression \"%s\"\n", file, line, expr);
    // std::exit(EXIT_FAILURE);
    std::abort(); // TODO
}
