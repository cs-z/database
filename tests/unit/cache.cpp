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
        call_count_++;
        return "value_" + std::to_string(key);
    }

    [[nodiscard]] std::size_t GetCallCount() const
    {
        return call_count_;
    }

private:
    std::size_t call_count_ = 0;
};

TEST(CacheUnitTest, OldestIsEvicted)
{
    static constexpr std::size_t  kCapacity = 4UL;
    Cache<Key, Value, MockLoader> cache{kCapacity, MockLoader{}};

    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 1);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 1);

    EXPECT_EQ(cache.Get(1), "value_1");
    EXPECT_EQ(cache.GetSize(), 2);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 2);

    EXPECT_EQ(cache.Get(2), "value_2");
    EXPECT_EQ(cache.GetSize(), 3);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 3);

    EXPECT_EQ(cache.Get(3), "value_3");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    // Cache is full
    EXPECT_EQ(cache.Get(4), "value_4");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 5);

    // Cache miss
    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 6);

    // Cache miss
    EXPECT_EQ(cache.Get(1), "value_1");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 7);
}

TEST(CacheUnitTest, HitPreventsEviction)
{
    static constexpr std::size_t  kCapacity = 4UL;
    Cache<Key, Value, MockLoader> cache{kCapacity, MockLoader{}};

    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 1);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 1);

    EXPECT_EQ(cache.Get(1), "value_1");
    EXPECT_EQ(cache.GetSize(), 2);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 2);

    EXPECT_EQ(cache.Get(2), "value_2");
    EXPECT_EQ(cache.GetSize(), 3);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 3);

    EXPECT_EQ(cache.Get(3), "value_3");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    // Cache hit
    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    // Cache miss
    EXPECT_EQ(cache.Get(4), "value_4");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 5);

    // Cache hit
    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 5);

    // Cache miss
    EXPECT_EQ(cache.Get(1), "value_1");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 6);
}

TEST(CacheUnitTest, RemoveExisting)
{
    static constexpr std::size_t  kCapacity = 4UL;
    Cache<Key, Value, MockLoader> cache{kCapacity, MockLoader{}};

    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 1);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 1);

    EXPECT_EQ(cache.Get(1), "value_1");
    EXPECT_EQ(cache.GetSize(), 2);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 2);

    EXPECT_EQ(cache.Get(2), "value_2");
    EXPECT_EQ(cache.GetSize(), 3);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 3);

    EXPECT_EQ(cache.Get(3), "value_3");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    cache.Remove(2);
    EXPECT_EQ(cache.GetSize(), 3);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    EXPECT_EQ(cache.Get(4), "value_4");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 5);

    EXPECT_EQ(cache.Get(3), "value_3");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 5);

    EXPECT_EQ(cache.Get(2), "value_2");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 6);
}

TEST(CacheUnitTest, RemoveNonExisting)
{
    static constexpr std::size_t  kCapacity = 4UL;
    Cache<Key, Value, MockLoader> cache{kCapacity, MockLoader{}};

    // Remove non-existent item
    cache.Remove(0);
    EXPECT_EQ(cache.GetSize(), 0);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 0);

    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 1);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 1);

    // Remove existent item
    cache.Remove(0);
    EXPECT_EQ(cache.GetSize(), 0);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 1);

    // Cache miss
    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 1);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 2);

    EXPECT_EQ(cache.Get(1), "value_1");
    EXPECT_EQ(cache.GetSize(), 2);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 3);
}

TEST(CacheUnitTest, ClearAll)
{
    static constexpr std::size_t  kCapacity = 4UL;
    Cache<Key, Value, MockLoader> cache{kCapacity, MockLoader{}};

    EXPECT_EQ(cache.Get(0), "value_0");
    EXPECT_EQ(cache.GetSize(), 1);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 1);

    EXPECT_EQ(cache.Get(1), "value_1");
    EXPECT_EQ(cache.GetSize(), 2);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 2);

    EXPECT_EQ(cache.Get(2), "value_2");
    EXPECT_EQ(cache.GetSize(), 3);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 3);

    EXPECT_EQ(cache.Get(3), "value_3");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    // Cache hit
    EXPECT_EQ(cache.Get(3), "value_3");
    EXPECT_EQ(cache.GetSize(), 4);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    cache.Clear();
    EXPECT_EQ(cache.GetSize(), 0);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 4);

    // Cache miss
    EXPECT_EQ(cache.Get(3), "value_3");
    EXPECT_EQ(cache.GetSize(), 1);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 5);

    cache.Remove(3);
    EXPECT_EQ(cache.GetSize(), 0);
    EXPECT_EQ(cache.GetLoader().GetCallCount(), 5);
}
