//
// Created by unite on 30.07.2026.
//

#ifndef NEKOLIB_FLAT_MAP_H
#define NEKOLIB_FLAT_MAP_H

#include <container/vector.h>

#include <cstddef>
#include <flat_map>
#include <functional>

namespace neko
{
/**
 * @brief SmallFlatMap is a std::flat_map whose key and value containers are both SmallVector: sorted,
 * binary-searched, and it never touches the heap.
 *
 * Use it where n is small and known-bounded. Do NOT use it as a content registry that grows with
 * authored assets -- that is what std::unordered_map is for.
 *
 * The default comparator is std::less<> (transparent), so a SmallFlatMap<std::string, V, N> is probed
 * AND inserted into with a std::string_view without materialising a std::string first: find,
 * contains, erase, operator[], try_emplace and insert_or_assign all take any key comparable with Key.
 *
 * The API is std::flat_map's, so find() returns an iterator and there is no capacity(); the bound is
 * keys().capacity(). Iteration is in KEY order, and erase preserves that order.
 *
 * ⚠️ AN INSERT THAT OVERFLOWS THROWS neko::CapacityError AND LEAVES THE MAP EMPTY. std::flat_map
 * restores its keys/values invariant by clearing both containers when an insert throws, and both MSVC's
 * and libc++'s implementations do exactly that (measured). So a caller that means to survive a full map
 * must test size() == keys().capacity() BEFORE inserting; catching the exception is not a recovery.
 *
 * @tparam Key
 * @tparam Value
 * @tparam Capacity
 * @tparam Compare
 */
template<typename Key, typename Value, std::size_t Capacity, typename Compare = std::less<>>
using SmallFlatMap = std::flat_map<Key, Value, Compare, SmallVector<Key, Capacity>, SmallVector<Value, Capacity>>;
}

#endif //NEKOLIB_FLAT_MAP_H
