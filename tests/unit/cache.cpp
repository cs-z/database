#include "cache.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using Key   = int;
using Value = std::string;

class MockLoader
{
public:
    [[nodiscard]] Value operator()(Key key)
    {
        callCount++;
        return "value_" + std::to_string(key);
    }

    [[nodiscard]] std::size_t getCallCount() const
    {
        return callCount;
    }

private:
    std::size_t callCount = 0;
};

TEST(CacheTest, OldestIsEvicted)
{
    static constexpr std::size_t  capacity = 4UL;
    Cache<Key, Value, MockLoader> cache{capacity, MockLoader{}};

    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 1);
    EXPECT_EQ(cache.getLoader().getCallCount(), 1);

    EXPECT_EQ(cache.get(1), "value_1");
    EXPECT_EQ(cache.getSize(), 2);
    EXPECT_EQ(cache.getLoader().getCallCount(), 2);

    EXPECT_EQ(cache.get(2), "value_2");
    EXPECT_EQ(cache.getSize(), 3);
    EXPECT_EQ(cache.getLoader().getCallCount(), 3);

    EXPECT_EQ(cache.get(3), "value_3");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    // Cache is full
    EXPECT_EQ(cache.get(4), "value_4");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 5);

    // Cache miss
    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 6);

    // Cache miss
    EXPECT_EQ(cache.get(1), "value_1");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 7);
}

TEST(CacheTest, HitPreventsEviction)
{
    static constexpr std::size_t  capacity = 4UL;
    Cache<Key, Value, MockLoader> cache{capacity, MockLoader{}};

    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 1);
    EXPECT_EQ(cache.getLoader().getCallCount(), 1);

    EXPECT_EQ(cache.get(1), "value_1");
    EXPECT_EQ(cache.getSize(), 2);
    EXPECT_EQ(cache.getLoader().getCallCount(), 2);

    EXPECT_EQ(cache.get(2), "value_2");
    EXPECT_EQ(cache.getSize(), 3);
    EXPECT_EQ(cache.getLoader().getCallCount(), 3);

    EXPECT_EQ(cache.get(3), "value_3");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    // Cache hit
    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    // Cache miss
    EXPECT_EQ(cache.get(4), "value_4");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 5);

    // Cache hit
    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 5);

    // Cache miss
    EXPECT_EQ(cache.get(1), "value_1");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 6);
}

TEST(CacheTest, RemoveExisting)
{
    static constexpr std::size_t  capacity = 4UL;
    Cache<Key, Value, MockLoader> cache{capacity, MockLoader{}};

    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 1);
    EXPECT_EQ(cache.getLoader().getCallCount(), 1);

    EXPECT_EQ(cache.get(1), "value_1");
    EXPECT_EQ(cache.getSize(), 2);
    EXPECT_EQ(cache.getLoader().getCallCount(), 2);

    EXPECT_EQ(cache.get(2), "value_2");
    EXPECT_EQ(cache.getSize(), 3);
    EXPECT_EQ(cache.getLoader().getCallCount(), 3);

    EXPECT_EQ(cache.get(3), "value_3");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    cache.remove(2);
    EXPECT_EQ(cache.getSize(), 3);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    EXPECT_EQ(cache.get(4), "value_4");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 5);

    EXPECT_EQ(cache.get(3), "value_3");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 5);

    EXPECT_EQ(cache.get(2), "value_2");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 6);
}

TEST(CacheTest, RemoveNonExisting)
{
    static constexpr std::size_t  capacity = 4UL;
    Cache<Key, Value, MockLoader> cache{capacity, MockLoader{}};

    // Remove non-existent item
    cache.remove(0);
    EXPECT_EQ(cache.getSize(), 0);
    EXPECT_EQ(cache.getLoader().getCallCount(), 0);

    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 1);
    EXPECT_EQ(cache.getLoader().getCallCount(), 1);

    // Remove existent item
    cache.remove(0);
    EXPECT_EQ(cache.getSize(), 0);
    EXPECT_EQ(cache.getLoader().getCallCount(), 1);

    // Cache miss
    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 1);
    EXPECT_EQ(cache.getLoader().getCallCount(), 2);

    EXPECT_EQ(cache.get(1), "value_1");
    EXPECT_EQ(cache.getSize(), 2);
    EXPECT_EQ(cache.getLoader().getCallCount(), 3);
}

TEST(CacheTest, ClearAll)
{
    static constexpr std::size_t  capacity = 4UL;
    Cache<Key, Value, MockLoader> cache{capacity, MockLoader{}};

    EXPECT_EQ(cache.get(0), "value_0");
    EXPECT_EQ(cache.getSize(), 1);
    EXPECT_EQ(cache.getLoader().getCallCount(), 1);

    EXPECT_EQ(cache.get(1), "value_1");
    EXPECT_EQ(cache.getSize(), 2);
    EXPECT_EQ(cache.getLoader().getCallCount(), 2);

    EXPECT_EQ(cache.get(2), "value_2");
    EXPECT_EQ(cache.getSize(), 3);
    EXPECT_EQ(cache.getLoader().getCallCount(), 3);

    EXPECT_EQ(cache.get(3), "value_3");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    // Cache hit
    EXPECT_EQ(cache.get(3), "value_3");
    EXPECT_EQ(cache.getSize(), 4);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    cache.clear();
    EXPECT_EQ(cache.getSize(), 0);
    EXPECT_EQ(cache.getLoader().getCallCount(), 4);

    // Cache miss
    EXPECT_EQ(cache.get(3), "value_3");
    EXPECT_EQ(cache.getSize(), 1);
    EXPECT_EQ(cache.getLoader().getCallCount(), 5);

    cache.remove(3);
    EXPECT_EQ(cache.getSize(), 0);
    EXPECT_EQ(cache.getLoader().getCallCount(), 5);
}
