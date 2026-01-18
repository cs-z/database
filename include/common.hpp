#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// helper for std::visit
template <typename... Ts> struct Overload : Ts...
{
    using Ts::operator()...;
};
template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;

// usef for type safety (typedef or 'using' would not prevent passing argument with mismatching
// type)
template <typename Tag, typename T> class StrongId
{
  public:
    using Type = T;

    constexpr StrongId() = default;
    explicit constexpr StrongId(T id) : id{id} {}

    constexpr StrongId(const StrongId&) noexcept  = default;
    constexpr StrongId(StrongId&&) noexcept       = default;
    StrongId& operator=(const StrongId&) noexcept = default;
    StrongId& operator=(StrongId&&) noexcept      = default;

    constexpr bool operator<(const StrongId& other) const { return id < other.id; }
    constexpr bool operator<(T other) const { return id < other; }
    constexpr bool operator<=(const StrongId& other) const { return id <= other.id; }
    constexpr bool operator<=(T other) const { return id <= other; }
    constexpr bool operator>(const StrongId& other) const { return id > other.id; }
    constexpr bool operator>(T other) const { return id > other; }
    constexpr bool operator>=(const StrongId& other) const { return id >= other.id; }
    constexpr bool operator>=(T other) const { return id >= other; }
    constexpr bool operator==(const StrongId& other) const { return id == other.id; }
    constexpr bool operator==(T other) const { return id == other; }

    friend constexpr StrongId operator+(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id + b.id);
    }
    friend constexpr StrongId operator+(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id + b);
    }
    friend constexpr StrongId operator+(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a + b.id);
    }

    friend constexpr StrongId operator-(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id - b.id);
    }
    friend constexpr StrongId operator-(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id - b);
    }
    friend constexpr StrongId operator-(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a - b.id);
    }

    friend constexpr StrongId operator*(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id * b.id);
    }
    friend constexpr StrongId operator*(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id * b);
    }
    friend constexpr StrongId operator*(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a * b.id);
    }

    friend constexpr StrongId operator/(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id / b.id);
    }
    friend constexpr StrongId operator/(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id / b);
    }
    friend constexpr StrongId operator/(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a / b.id);
    }

    friend constexpr StrongId operator%(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id % b.id);
    }
    friend constexpr StrongId operator%(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id % b);
    }
    friend constexpr StrongId operator%(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a % b.id);
    }

    constexpr StrongId operator++(int) { return StrongId{id++}; }
    constexpr StrongId operator--(int) { return StrongId{id--}; }

    [[nodiscard]] constexpr T get() const { return id; }
    [[nodiscard]] std::string to_string() const { return std::to_string(id); }

  private:
    T id;
};

namespace std
{
template <typename Tag, typename T> struct hash<StrongId<Tag, T>>
{
    std::size_t operator()(const StrongId<Tag, T>& id) const { return std::hash<T>{}(id.get()); }
};
}  // namespace std

struct ColumnTag
{
};
using ColumnId = StrongId<ColumnTag, unsigned int>;

[[noreturn]] void abort_expr(const char* expr, const char* file, long line);

#define ASSERT(expr) static_cast<bool>(expr) ? void(0) : abort_expr(#expr, __FILE__, __LINE__)
#define UNREACHABLE() abort_expr("UNREACHABLE", __FILE__, __LINE__)

#define FLEXIBLE_ARRAY 1

template <typename T> constexpr T align_up(T value, T align)
{
    ASSERT(align > 0);
    const T rem = value % align;
    if (rem)
    {
        value += align - rem;
    }
    return value;
}

template <typename T> constexpr T align_down(T value, T align)
{
    ASSERT(align > 0);
    const T rem = value % align;
    if (rem)
    {
        value -= rem;
    }
    return value;
}
