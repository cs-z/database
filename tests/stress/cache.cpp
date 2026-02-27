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
    static constexpr std::size_t seed           = 42UL;
    static constexpr std::size_t capacity       = 100UL;
    static constexpr std::size_t operationCount = 25'000UL;

    auto loader = [](Key key) -> Value { return "value_" + std::to_string(key); };

    Cache<Key, Value, decltype(loader)> cache{capacity, loader};

    std::mt19937 rng{seed};

    static constexpr Key               minKey = 1;
    static constexpr Key               maxKey = 300;
    std::uniform_int_distribution<Key> keyDist{minKey, maxKey};

    static constexpr int               percentMin    = 1;
    static constexpr int               percentGet    = 90;
    static constexpr int               percentRemove = 99;
    static constexpr int               percentMax    = 100;
    std::uniform_int_distribution<int> operationDist{percentMin, percentMax};

    for ([[maybe_unused]] const auto i : std::views::iota(0UL, operationCount))
    {
        const auto operation = operationDist(rng);
        const auto key       = keyDist(rng);
        if (operation <= percentGet)
        {
            const auto& value = cache.get(key);
            EXPECT_EQ(value, loader(key));
        }
        else if (operation <= percentRemove)
        {
            cache.remove(key);
        }
        else
        {
            cache.clear();
            EXPECT_EQ(cache.getSize(), 0);
        }
        EXPECT_LE(cache.getSize(), capacity);
    }
}
