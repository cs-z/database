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
    Cache(std::size_t capacity, Loader loader) : capacity_{capacity}, loader_{std::move(loader)}
    {
        assert(capacity > 0);
    }

    [[nodiscard]] std::size_t GetCapacity() const
    {
        return capacity_;
    }

    [[nodiscard]] std::size_t GetSize() const
    {
        return cache_.size();
    }

    [[nodiscard]] const Loader& GetLoader() const
    {
        return loader_;
    }

    [[nodiscard]] Value& Get(const Key& key)
    {
        // Cache hit
        if (auto it = cache_.find(key); it != cache_.end())
        {
            pairs_.splice(pairs_.begin(), pairs_, it->second);
            return it->second->second;
        }
        // Cache miss
        auto value = loader_(key);
        if (GetSize() < capacity_)
        {
            // Create new node
            pairs_.emplace_front(key, std::move(value));
        }
        else
        {
            // Reuse last node
            auto last = std::prev(pairs_.end());
            cache_.erase(last->first);
            last->first  = key;
            last->second = std::move(value);
            pairs_.splice(pairs_.begin(), pairs_, last);
        }
        // New node is at front
        cache_.emplace(key, pairs_.begin());
        return pairs_.front().second;
    }

    void Remove(const Key& key)
    {
        if (auto it = cache_.find(key); it != cache_.end())
        {
            pairs_.erase(it->second);
            cache_.erase(it);
        }
    }

    void Clear()
    {
        cache_.clear();
        pairs_.clear();
    }

private:
    using Pair = std::pair<Key, Value>;
    using Iter = typename std::list<Pair>::iterator;

    std::size_t capacity_;
    Loader      loader_;

    std::list<Pair>               pairs_;
    std::unordered_map<Key, Iter> cache_;
};
