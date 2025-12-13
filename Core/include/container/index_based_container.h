#ifndef NEKOLIB_INDEX_BASED_CONTAINER_H
#define NEKOLIB_INDEX_BASED_CONTAINER_H
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <memory>

namespace neko
{

/**
 * @brief Concept that verifies a type can be marked as invalid.
 *
 * Types satisfying CanBeInvalid must provide:
 * - A method `bool IsInvalid() const` to check if the value is in an invalid state
 * - A static factory method `static T GenerateInvalidValue()` to create an invalid instance
 *
 * This concept is required for types stored in IndexBasedContainer, as the container
 * uses invalid values to mark removed elements.
 *
 * @tparam T The type to validate
 *
 * Example implementation:
 * @code
 * struct Entity {
 *   int id = -1;
 *
 *   bool IsInvalid() const {
 *     return id < 0;
 *   }
 *
 *   static Entity GenerateInvalidValue() {
 *     return Entity{-1};
 *   }
 * };
 * @endcode
 */
template <typename T>
concept CanBeInvalid = requires(const T value)
{
    { value.IsInvalid() } -> std::same_as<bool>;
    { T::GenerateInvalidValue() } -> std::convertible_to<T>;
};

/**
 * @brief Generational index for safe, stable references into IndexBasedContainer.
 *
 * Index combines an array position with a generation counter to detect stale accesses.
 * When an element is removed from an IndexBasedContainer, the generation at that slot
 * is incremented. Old Index values will have mismatched generations and fail validation.
 *
 * This pattern prevents the "dangling index" problem where:
 * 1. Get index to element A at position 5
 * 2. Remove element A
 * 3. Add new element B at position 5
 * 4. Old index (5, gen=0) would incorrectly access element B
 *
 * With generations:
 * 1. Get index to element A: Index(5, gen=0)
 * 2. Remove element A: generation at slot 5 incremented to 1
 * 3. Add new element B at position 5 with generation 1
 * 4. Old index Index(5, gen=0) fails validation (0 != 1)
 *
 * @tparam T The element type (used for type safety)
 * @tparam IndexType The underlying type for the array index (default: int)
 * @tparam GenerationIndexType The underlying type for the generation counter (default: int)
 */
template <typename T, typename IndexType=int, typename GenerationIndexType=int>
class Index
{
public:
    using index_type = IndexType;
    using generation_index_type = GenerationIndexType;

    /**
     * @brief Constructs an Index with the specified position and generation.
     *
     * @param index The array position
     * @param generationIndex The generation counter (default: 0 for new elements)
     */
    constexpr explicit Index(IndexType index, GenerationIndexType generationIndex = 0) :
        index_(index), generationIndex_(generationIndex)
    {
    }

    /**
     * @brief Three-way comparison operator for Index ordering.
     *
     * Compares indices first by array position, then by generation.
     *
     * @param index The Index to compare against
     * @return Ordering result
     */
    [[nodiscard]] constexpr auto operator<=>(const Index& index) const = default;

    /**
     * @brief Gets the array position component of the index.
     *
     * @return The array position (does not include bounds checking)
     */
    [[nodiscard]] constexpr auto index() const noexcept {return index_;}

    /**
     * @brief Gets the generation counter component of the index.
     *
     * @return The generation counter
     */
    [[nodiscard]] constexpr auto generationIndex() const noexcept {return generationIndex_;}

private:
    template <typename U, typename V>
    friend class IndexBasedContainer;
    IndexType index_ = -1;
    GenerationIndexType generationIndex_ = 0;
};

/**
 * @brief Helper type alias for creating allocator types for IndexBasedContainer.
 *
 * @tparam AllocatorT Template template parameter for the allocator
 * @tparam T The element type
 */
template <template <typename> class AllocatorT, typename T>
using indexed_container_allocator_type = AllocatorT<std::pair<T, typename Index<T>::generation_index_type>>;

/**
 * @brief Container that provides stable indices to elements via generational indexing.
 *
 * IndexBasedContainer solves the "stable reference" problem for dynamically sized containers.
 * Unlike std::vector where indices become invalid after erasure, this container maintains
 * stable Index handles that detect stale accesses via generation counters.
 *
 * Key features:
 * - Stable indices that remain valid across removals (but detect stale access)
 * - Automatic slot reuse for removed elements
 * - Generation counters prevent use-after-free bugs
 * - Custom allocator support
 * - Random-access iteration (includes invalid elements)
 *
 * How it works:
 * - Elements are stored with generation counters: std::pair<T, generation>
 * - add() searches for invalidated slots to reuse before growing
 * - remove() marks the slot as invalid and increments its generation
 * - Access via Index validates the generation before returning the element
 *
 * Performance characteristics:
 * - add(): O(n) due to linear search for invalid slots
 * - remove(): O(1) for invalidation
 * - operator[]/at(): O(1) for access + generation check
 * - size(): O(n) to count non-invalid elements
 *
 * Memory management:
 * - The container never shrinks (removed elements remain as invalid placeholders)
 * - Use clear() to fully reset the container
 * - Iterators include both valid and invalid elements
 *
 * Thread safety:
 * - Not thread-safe by default
 * - External synchronization required for concurrent access
 *
 * @tparam T The element type (must satisfy CanBeInvalid concept)
 * @tparam AllocatorT The allocator type (default: std::allocator)
 *
 * Example usage:
 * @code
 * struct Entity {
 *   int id = -1;
 *   std::string name;
 *
 *   bool IsInvalid() const { return id < 0; }
 *   static Entity GenerateInvalidValue() { return Entity{-1, ""}; }
 * };
 *
 * IndexBasedContainer<Entity> entities;
 *
 * // Add elements
 * Index<Entity> playerIdx = entities.add(Entity{1, "Player"});
 * Index<Entity> enemyIdx = entities.add(Entity{2, "Enemy"});
 *
 * // Access elements
 * entities[playerIdx].name = "Hero";
 *
 * // Remove element
 * entities.remove(enemyIdx);
 *
 * // Stale access detected
 * try {
 *   entities[enemyIdx];  // Throws: generation mismatch
 * } catch (const std::out_of_range&) {
 *   // Handle stale index
 * }
 * @endcode
 */
template <typename T, typename AllocatorT=std::allocator<std::pair<T, typename Index<T>::generation_index_type>>>
class IndexBasedContainer
{
public:
    /**
     * @brief Default constructor. Creates an empty container with default allocator.
     */
    IndexBasedContainer() = default;

    /**
     * @brief Constructs a container with a copy of the given allocator.
     *
     * @param allocator The allocator to use
     */
    explicit IndexBasedContainer(const AllocatorT& allocator) : values_(allocator)
    {
    }

    /**
     * @brief Constructs a container by moving the given allocator.
     *
     * @param allocator The allocator to move
     */
    explicit IndexBasedContainer(AllocatorT&& allocator) : values_(std::move(allocator))
    {
    }

    /**
     * @brief Adds an element to the container by moving it.
     *
     * Searches for an invalid slot to reuse. If found, the new value is moved into that slot.
     * Otherwise, the value is appended to the end.
     *
     * @param new_value The element to add (moved)
     * @return Index<T> A generational index referring to the added element
     *
     * Performance: O(n) due to linear search for invalid slots
     */
    Index<T> add(T&& new_value)
    {
        auto it = std::find_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return v.first.IsInvalid();
        });
        if (it == values_.end())
        {
            Index<T> index{static_cast<Index<T>::index_type>(std::ssize(values_))};
            values_.push_back({std::move(new_value), 0});
            return index;
        }
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it))};
        it->first = std::move(new_value);
        return index;
    }

    /**
     * @brief Adds an element to the container by copying it.
     *
     * Searches for an invalid slot to reuse. If found, the new value is copied into that slot
     * and its generation is incremented. Otherwise, the value is appended with generation 0.
     *
     * @param new_value The element to add (copied)
     * @return Index<T> A generational index referring to the added element
     *
     * Performance: O(n) due to linear search for invalid slots
     */
    Index<T> add(const T& new_value)
    {
        auto it = std::find_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return v.first.IsInvalid();
        });
        if (it == values_.end())
        {
            Index<T> index{static_cast<Index<T>::index_type>(std::ssize(values_))};
            values_.push_back({new_value, 0});
            return index;
        }
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it)), it->second + 1};
        it->first = new_value;
        return index;
    }

    /**
     * @brief Adds a default-constructed element to the container.
     *
     * Searches for an invalid slot to reuse. If found, a default-constructed T{} is placed there.
     * Otherwise, T{} is appended to the end.
     *
     * @return Index<T> A generational index referring to the added element
     *
     * Performance: O(n) due to linear search for invalid slots
     */
    Index<T> add()
    {
        auto it = std::find_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return v.first.IsInvalid();
        });
        if (it == values_.end())
        {
            Index<T> index{static_cast<Index<T>::index_type>(std::ssize(values_))};
            values_.push_back({{}, 0});
            return index;
        }
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it)), it->second + 1};
        it->first = {};
        return index;
    }

    /**
     * @brief Accesses an element by index with generation validation.
     *
     * @param index The generational index
     * @return T& Mutable reference to the element
     * @throws std::out_of_range if generation mismatch (stale index)
     *
     * Note: Does NOT perform bounds checking on the array index itself
     * Performance: O(1)
     */
    [[nodiscard]] T& operator[](Index<T> index)
    {
        auto& pair = values_[index.index_];
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong generation index");
        }
        return pair.first;
    }

    /**
     * @brief Accesses an element by index with generation validation (const version).
     *
     * @param index The generational index
     * @return const T& Const reference to the element
     * @throws std::out_of_range if generation mismatch (stale index)
     *
     * Note: Does NOT perform bounds checking on the array index itself
     * Performance: O(1)
     */
    [[nodiscard]] const T& operator[](Index<T> index) const
    {
        const auto& pair = values_[index.index_];
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong Generation Index");
        }
        return pair.first;
    }

    /**
     * @brief Accesses an element by index with bounds and generation validation (const version).
     *
     * @param index The generational index
     * @return const T& Const reference to the element
     * @throws std::out_of_range if index out of bounds or generation mismatch
     *
     * Performance: O(1)
     */
    [[nodiscard]] const T& at(Index<T> index) const
    {
        const auto& pair = values_.at(index.index_);
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong Generation Index");
        }
        return pair.first;
    }

    /**
     * @brief Accesses an element by index with bounds and generation validation.
     *
     * @param index The generational index
     * @return T& Mutable reference to the element
     * @throws std::out_of_range if index out of bounds or generation mismatch
     *
     * Performance: O(1)
     */
    [[nodiscard]] T& at(Index<T> index)
    {
        auto& pair = values_.at(index.index_);
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong generation index");
        }
        return pair.first;
    }

    /**
     * @brief Checks if an index refers to a valid (non-removed) element.
     *
     * Validates both the generation and that the element is not marked invalid.
     *
     * @param index The generational index to check
     * @return true if the index is valid and element is not invalid, false otherwise
     * @throws std::out_of_range if index.index_ is out of bounds
     *
     * Performance: O(1)
     */
    [[nodiscard]] constexpr bool contains(Index<T> index) const
    {
        const auto& pair = values_.at(index.index_);
        return index.generationIndex_ == pair.second && !pair.first.IsInvalid();
    }

    /**
     * @brief Removes an element from the container.
     *
     * Marks the element as invalid and increments the generation counter at that slot.
     * The slot becomes available for reuse by future add() calls.
     * The original Index becomes stale and will fail validation on future access.
     *
     * @param index The generational index of the element to remove
     * @throws std::out_of_range if index out of bounds or generation mismatch
     *
     * Performance: O(1)
     *
     * Example:
     * @code
     * auto idx = container.add(value);
     * container.remove(idx);
     * // idx is now stale - accessing it will throw
     * @endcode
     */
    void remove(Index<T> index)
    {
        auto& pair = values_.at(index.index_);
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong generation index");
        }
        pair.first = T::GenerateInvalidValue();
        ++pair.second;
    }

    /**
     * @brief Returns the number of valid (non-invalid) elements in the container.
     *
     * This counts only elements that are NOT marked as invalid. The internal storage
     * may be larger due to removed elements being kept as invalid placeholders.
     *
     * @return size_t The count of valid elements
     *
     * Performance: O(n) - must iterate to count non-invalid elements
     */
    [[nodiscard]] size_t size() const noexcept
    {
        return std::count_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return !v.first.IsInvalid();
        });
    }

    /**
     * @brief Random-access iterator for IndexBasedContainer.
     *
     * Provides iteration over ALL elements in the container, including invalid ones.
     * Users must check element.IsInvalid() when iterating to skip removed elements.
     *
     * Important: This iterator does NOT automatically skip invalid elements.
     * It provides raw access to the underlying storage.
     *
     * Example:
     * @code
     * for (auto it = container.begin(); it != container.end(); ++it) {
     *   if (!it->IsInvalid()) {
     *     // Process valid element
     *   }
     * }
     * @endcode
     */
    class Iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        using pair_type = std::pair<T, typename Index<T>::generation_index_type>;

        explicit Iterator(pair_type* ptr) : m_ptr(ptr)
        {
        }

        reference operator*() const { return m_ptr->first; }
        pointer operator->() const { return &m_ptr->first; }

        Iterator& operator++()
        {
            ++m_ptr;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator& operator--()
        {
            --m_ptr;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        Iterator operator+(difference_type n) const { return Iterator(m_ptr + n); }
        Iterator operator-(difference_type n) const { return Iterator(m_ptr - n); }
        difference_type operator-(const Iterator& other) const { return m_ptr - other.m_ptr; }
        bool operator==(const Iterator& other) const { return m_ptr == other.m_ptr; }
        bool operator!=(const Iterator& other) const { return m_ptr != other.m_ptr; }
        bool operator<(const Iterator& other) const { return m_ptr < other.m_ptr; }
        bool operator>(const Iterator& other) const { return m_ptr > other.m_ptr; }
        bool operator<=(const Iterator& other) const { return m_ptr <= other.m_ptr; }
        bool operator>=(const Iterator& other) const { return m_ptr >= other.m_ptr; }

    private:
        pair_type* m_ptr;
    };

    /**
     * @brief Returns an iterator to the beginning of the container.
     *
     * @return Iterator pointing to the first element (may be invalid)
     */
    auto begin()
    {
        return Iterator{values_.data()};
    }

    /**
     * @brief Returns an iterator to the end of the container.
     *
     * @return Iterator pointing past the last element
     */
    auto end()
    {
        return Iterator{values_.data() + values_.size()};
    }

    /**
     * @brief Removes all elements from the container.
     *
     * Clears the underlying storage completely. This is the only way to truly
     * shrink the container's memory footprint.
     */
    void clear()
    {
        values_.clear();
    }

    /**
     * @brief Returns a copy of the allocator.
     *
     * @return The allocator associated with the container
     */
    auto allocator() const noexcept { return values_.get_allocator(); }

private:
    static_assert(CanBeInvalid<T>, "requires function bool IsInvalid() && GenerateInvalidValue();");
    std::vector<std::pair<T, typename Index<T>::generation_index_type>, AllocatorT> values_;
};
}

#endif //NEKOLIB_INDEX_BASED_CONTAINER_H
