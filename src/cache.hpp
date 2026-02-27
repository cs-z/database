#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

// In memory fixed-capacity LRU cache.

template <typename Key, typename Value, typename Loader>
    requires std::copyable<Key> && std::equality_comparable<Key> && std::movable<Value> &&
             std::move_constructible<Loader> && requires(const Key& key, Loader& loader) {
                 { std::hash<Key>{}(key) } -> std::convertible_to<std::size_t>;
                 { loader(key) } -> std::convertible_to<Value>;
             }
class Cache
{
public:
    Cache(std::size_t capacity, Loader loader) : capacity{capacity}, loader{std::move(loader)}
    {
        assert(capacity > 0);
    }

    [[nodiscard]] std::size_t getCapacity() const
    {
        return capacity;
    }

    [[nodiscard]] std::size_t getSize() const
    {
        return cache.size();
    }

    [[nodiscard]] const Loader& getLoader() const
    {
        return loader;
    }

    [[nodiscard]] Value& get(const Key& key)
    {
        // Cache hit
        if (auto it = cache.find(key); it != cache.end())
        {
            pairs.splice(pairs.begin(), pairs, it->second);
            return it->second->second;
        }
        // Cache miss
        auto value = loader(key);
        if (getSize() < capacity)
        {
            // Create new node
            pairs.emplace_front(key, std::move(value));
        }
        else
        {
            // Reuse last node
            auto last = std::prev(pairs.end());
            cache.erase(last->first);
            last->first  = key;
            last->second = std::move(value);
            pairs.splice(pairs.begin(), pairs, last);
        }
        // New node is at front
        cache.emplace(key, pairs.begin());
        return pairs.front().second;
    }

    void remove(const Key& key)
    {
        if (auto it = cache.find(key); it != cache.end())
        {
            pairs.erase(it->second);
            cache.erase(it);
        }
    }

    void clear()
    {
        cache.clear();
        pairs.clear();
    }

private:
    using Pair = std::pair<Key, Value>;
    using Iter = typename std::list<Pair>::iterator;

    std::size_t capacity;
    Loader      loader;

    std::list<Pair>               pairs;
    std::unordered_map<Key, Iter> cache;
};
