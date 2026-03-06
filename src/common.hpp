#pragma once

#include <cstdint>
#include <string>

using U8  = std::uint8_t;
using U16 = std::uint16_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;

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
    explicit constexpr StrongId(T id) : id_{id}
    {
    }

    constexpr StrongId(const StrongId&) noexcept  = default;
    constexpr StrongId(StrongId&&) noexcept       = default;
    StrongId& operator=(const StrongId&) noexcept = default;
    StrongId& operator=(StrongId&&) noexcept      = default;
    ~StrongId()                                   = default;

    constexpr bool operator<(const StrongId& other) const
    {
        return id_ < other.id_;
    }
    constexpr bool operator<(T other) const
    {
        return id_ < other;
    }
    constexpr bool operator<=(const StrongId& other) const
    {
        return id_ <= other.id_;
    }
    constexpr bool operator<=(T other) const
    {
        return id_ <= other;
    }
    constexpr bool operator>(const StrongId& other) const
    {
        return id_ > other.id_;
    }
    constexpr bool operator>(T other) const
    {
        return id_ > other;
    }
    constexpr bool operator>=(const StrongId& other) const
    {
        return id_ >= other.id_;
    }
    constexpr bool operator>=(T other) const
    {
        return id_ >= other;
    }
    constexpr bool operator==(const StrongId& other) const
    {
        return id_ == other.id_;
    }
    constexpr bool operator==(T other) const
    {
        return id_ == other;
    }

    friend constexpr StrongId operator+(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id_ + b.id_);
    }
    friend constexpr StrongId operator+(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id_ + b);
    }
    friend constexpr StrongId operator+(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a + b.id_);
    }

    friend constexpr StrongId operator-(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id_ - b.id_);
    }
    friend constexpr StrongId operator-(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id_ - b);
    }
    friend constexpr StrongId operator-(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a - b.id_);
    }

    friend constexpr StrongId operator*(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id_ * b.id_);
    }
    friend constexpr StrongId operator*(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id_ * b);
    }
    friend constexpr StrongId operator*(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a * b.id_);
    }

    friend constexpr StrongId operator/(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id_ / b.id_);
    }
    friend constexpr StrongId operator/(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id_ / b);
    }
    friend constexpr StrongId operator/(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a / b.id_);
    }

    friend constexpr StrongId operator%(const StrongId& a, const StrongId& b)
    {
        return static_cast<StrongId>(a.id_ % b.id_);
    }
    friend constexpr StrongId operator%(const StrongId& a, T b)
    {
        return static_cast<StrongId>(a.id_ % b);
    }
    friend constexpr StrongId operator%(T a, const StrongId& b)
    {
        return static_cast<StrongId>(a % b.id_);
    }

    constexpr StrongId operator++(int)
    {
        return StrongId{id_++};
    }
    constexpr StrongId operator--(int)
    {
        return StrongId{id_--};
    }

    [[nodiscard]] constexpr T Get() const
    {
        return id_;
    }
    [[nodiscard]] std::string ToString() const
    {
        return std::to_string(id_);
    }

private:
    T id_;
};

namespace std
{
template <typename Tag, typename T> struct hash<StrongId<Tag, T>>
{
    std::size_t operator()(const StrongId<Tag, T>& id) const
    {
        return std::hash<T>{}(id.Get());
    }
};
} // namespace std

struct ColumnTag
{
};
using ColumnId = StrongId<ColumnTag, unsigned int>;

[[noreturn]] void AbortExpr(const char* expr, const char* file, std::int64_t line);

#define ASSERT(expr) static_cast<bool>(expr) ? void(0) : AbortExpr(#expr, __FILE__, __LINE__)
#define UNREACHABLE() AbortExpr("UNREACHABLE", __FILE__, __LINE__)

constexpr std::size_t kFlexibleArray = 1;

template <typename T> constexpr T AlignUp(T value, T align)
{
    ASSERT(align > 0);
    const T rem = value % align;
    if (rem)
    {
        value += align - rem;
    }
    return value;
}

template <typename T> constexpr T AlignDown(T value, T align)
{
    ASSERT(align > 0);
    const T rem = value % align;
    if (rem)
    {
        value -= rem;
    }
    return value;
}
