#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <variant>
#include <unordered_map>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// helper for std::visit
template<typename ... Ts>
struct Overload : Ts... { using Ts::operator()...; };
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

[[noreturn]] void abort_expr(const char *expr, const char *file, long line);

#define ASSERT(expr) static_cast<bool>(expr) ? void (0) : abort_expr(#expr, __FILE__, __LINE__)
#define UNREACHABLE() abort_expr("UNREACHABLE", __FILE__, __LINE__)
