//
// Created by unite on 30.07.2026.
//

#ifndef NEKOLIB_FLAT_MAP_H
#define NEKOLIB_FLAT_MAP_H

#include <container/vector.h>

#include <cstddef>
#include <utility>

namespace neko
{
/**
 * @brief SmallFlatMap is an associative container over a SmallVector of key/value pairs, searched
 * linearly. It never touches the heap, at the cost of O(n) lookup.
 *
 * Use it where n is small and known-bounded: at the handful-of-entries sizes this is built for, a
 * linear scan over contiguous pairs beats hashing plus a node chase, and unlike std::unordered_map
 * (or std::flat_map, which is heap-backed by default and absent from libc++ 19) it allocates
 * nothing. Do NOT use it as a content registry that grows with authored assets -- that is what
 * std::unordered_map is for.
 *
 * Lookup is heterogeneous: find/contains/erase take any key type comparable with Key, so a
 * SmallFlatMap<std::string, V, N> can be probed with a std::string_view without materialising a
 * std::string. Insertion still needs a real Key.
 *
 * Iteration order is insertion order, and erase() swaps the last element into the hole rather than
 * shifting, so it does not preserve that order. Every constraint on the element type in
 * SmallVector's storage invariant applies here too -- read it before choosing Key/Value.
 *
 * @tparam Key
 * @tparam Value
 * @tparam Capacity
 */
template<typename Key, typename Value, std::size_t Capacity>
    class SmallFlatMap
    {
    public:
        using value_type = std::pair<Key, Value>;

        constexpr SmallFlatMap() = default;

        [[nodiscard]] constexpr auto begin() { return items_.begin(); }
        [[nodiscard]] constexpr auto end() { return items_.end(); }
        [[nodiscard]] constexpr auto begin() const { return items_.begin(); }
        [[nodiscard]] constexpr auto end() const { return items_.end(); }

        /**
         * @brief Locates a value by any key comparable with Key.
         * @return a pointer to the mapped value, or nullptr when the key is absent
         */
        template<typename K>
        [[nodiscard]] constexpr Value* find(const K& key)
        {
            for(std::size_t i = 0; i < items_.size(); i++)
            {
                if(items_[i].first == key)
                {
                    return &items_[i].second;
                }
            }
            return nullptr;
        }

        template<typename K>
        [[nodiscard]] constexpr const Value* find(const K& key) const
        {
            for(std::size_t i = 0; i < items_.size(); i++)
            {
                if(items_[i].first == key)
                {
                    return &items_[i].second;
                }
            }
            return nullptr;
        }

        template<typename K>
        [[nodiscard]] constexpr bool contains(const K& key) const
        {
            return find(key) != nullptr;
        }

        /**
         * @brief Returns the value for key, default-constructing an entry if it is absent.
         * @throw std::out_of_range if a new entry is needed and the container is full
         */
        [[nodiscard]] constexpr Value& operator[](const Key& key)
        {
            if(Value* existing = find(key); existing != nullptr)
            {
                return *existing;
            }
            items_.push_back(value_type{key, Value{}});
            return items_.back().second;
        }

        /**
         * @brief Insert-or-assign that reports overflow instead of throwing, for callers whose
         * capacity is a data bound rather than an invariant.
         * @return true if the value was stored, false if the key was absent and the container full
         */
        [[nodiscard]] constexpr bool try_insert_or_assign(const Key& key, const Value& value)
        {
            if(Value* existing = find(key); existing != nullptr)
            {
                *existing = value;
                return true;
            }
            return items_.try_push_back(value_type{key, value});
        }

        /**
         * @brief Removes the entry for key, if any. Swaps the last element into the hole, so
         * insertion order is not preserved.
         * @return true if an entry was removed
         */
        template<typename K>
        constexpr bool erase(const K& key)
        {
            for(std::size_t i = 0; i < items_.size(); i++)
            {
                if(items_[i].first == key)
                {
                    const std::size_t last = items_.size() - 1;
                    if(i != last)
                    {
                        items_[i] = std::move(items_[last]);
                    }
                    items_.pop_back();
                    return true;
                }
            }
            return false;
        }

        constexpr void clear() { items_.clear(); }

        [[nodiscard]] constexpr auto size() const { return items_.size(); }
        [[nodiscard]] constexpr bool empty() const { return items_.empty(); }
        [[nodiscard]] constexpr bool is_full() const { return items_.is_full(); }
        [[nodiscard]] static constexpr auto capacity() noexcept { return Capacity; }

    private:
        SmallVector<value_type, Capacity> items_;
    };
}

#endif //NEKOLIB_FLAT_MAP_H
