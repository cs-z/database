#include "cache.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <random>
#include <ranges>
#include <string>

using Key   = int;
using Value = std::string;

TEST(CacheStressTest, RandomizedOperations)
{
    static constexpr std::size_t kSeed           = 42UL;
    static constexpr std::size_t kCapacity       = 100UL;
    static constexpr std::size_t kOperationCount = 25'000UL;

    auto loader = [](Key key) -> Value { return "value_" + std::to_string(key); };

    Cache<Key, Value, decltype(loader)> cache{kCapacity, loader};

    std::mt19937 rng{kSeed};

    static constexpr Key               kMinKey = 1;
    static constexpr Key               kMaxKey = 300;
    std::uniform_int_distribution<Key> key_dist{kMinKey, kMaxKey};

    static constexpr int               kPercentMin    = 1;
    static constexpr int               kPercentGet    = 90;
    static constexpr int               kPercentRemove = 99;
    static constexpr int               kPercentMax    = 100;
    std::uniform_int_distribution<int> operation_dist{kPercentMin, kPercentMax};

    for ([[maybe_unused]] const auto i : std::views::iota(0UL, kOperationCount))
    {
        const auto operation = operation_dist(rng);
        const auto key       = key_dist(rng);
        if (operation <= kPercentGet)
        {
            const auto& value = cache.Get(key);
            EXPECT_EQ(value, loader(key));
        }
        else if (operation <= kPercentRemove)
        {
            cache.Remove(key);
        }
        else
        {
            cache.Clear();
            EXPECT_EQ(cache.GetSize(), 0);
        }
        EXPECT_LE(cache.GetSize(), kCapacity);
    }
}
