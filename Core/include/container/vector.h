//
// Created by unite on 05.06.2024.
//

#ifndef NEKOLIB_FIXED_VECTOR_H
#define NEKOLIB_FIXED_VECTOR_H

#include <algorithm>
#include <cassert>
#include <charconv>
#include <compare>
#include <concepts>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace neko
{
/**
 * @brief Thrown by SmallVector when an operation needs more than Capacity elements.
 *
 * It derives from std::bad_alloc because that is what std::inplace_vector throws, so a caller written
 * against the standard catches it. But a bare bad_alloc reads as "the heap is exhausted" in a log --
 * and this is never that, it is a fixed bound that was hit -- so what() names the operation and the
 * capacity instead.
 *
 * It holds no std::string on purpose: it is thrown exactly when a bound is reached, so building its
 * message must not allocate. The text is formatted once, into a fixed buffer, at construction.
 */
class CapacityError : public std::bad_alloc
{
public:
    CapacityError(const char* operation, std::size_t capacity) noexcept
    {
        constexpr std::string_view prefix = "neko::SmallVector::";
        constexpr std::string_view middle = ": over capacity ";
        char* out = message_;
        char* const last = message_ + sizeof(message_) - 1;
        const auto append = [&out, last](std::string_view text)
        {
            const auto count = std::min(text.size(), static_cast<std::size_t>(last - out));
            out = std::copy_n(text.data(), count, out);
        };
        append(prefix);
        append(operation);
        append(middle);
        out = std::to_chars(out, last, capacity).ptr;
        *out = '\0';
    }

    [[nodiscard]] const char* what() const noexcept override { return message_; }

private:
    char message_[96]{};
};

namespace detail
{
[[noreturn]] inline void ThrowCapacityError(const char* operation, std::size_t capacity)
{
    throw CapacityError(operation, capacity);
}
} // namespace detail

/**
 * @brief SmallVector is std::inplace_vector (C++26) ahead of the standard library shipping it: a
 * contiguous vector with a fixed capacity, stored inline, that never touches the heap.
 *
 * Lifetime model -- the standard one. Only [0, size()) holds live objects; the rest of the storage is
 * uninitialised. push_back/emplace constructs into raw storage, pop_back/erase/clear destroy, a copy
 * copies size() elements, and T need not be default-constructible. SmallVector<T, N> is trivially
 * copyable whenever T is.
 *
 * The storage is an anonymous union holding a T[N]. That is the libc++ shape for this container: the
 * union suppresses construction of the array, the elements stay visible to a debugger, and pointer
 * arithmetic over them is well-defined. (A project rule against unions in hash/equality key types
 * does not apply -- this is uninitialised storage, not a key or a sum type.)
 *
 * Differences from std::inplace_vector, all deliberate:
 * - Overflow throws neko::CapacityError, which IS a std::bad_alloc, with a message naming the bound.
 * - operator[], front(), back(), pop_back() and the unchecked_* family assert their preconditions in
 *   Debug. pop_back() on an empty container is also a no-op in Release rather than undefined.
 * - Capacity 0 still reserves one T of storage (a zero-length array is ill-formed); capacity() is 0.
 *
 * ⚠️ Reading an element at or past size() reads DEAD STORAGE. The container this replaced kept every
 * slot a live, default-constructed T, so such a read used to return T{} silently.
 * @tparam T
 * @tparam Capacity
 */
template<typename T, std::size_t Capacity>
class SmallVector
{
    // Declared ahead of the special members because a requires-clause is not a complete-class context.
    static constexpr bool kTriviallyCopyAssignable = std::is_trivially_copy_constructible_v<T> &&
                                                     std::is_trivially_copy_assignable_v<T> &&
                                                     std::is_trivially_destructible_v<T>;
    static constexpr bool kTriviallyMoveAssignable = std::is_trivially_move_constructible_v<T> &&
                                                     std::is_trivially_move_assignable_v<T> &&
                                                     std::is_trivially_destructible_v<T>;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // ---- construction ------------------------------------------------------------------------------

    // Must stay constexpr: constant-initialised globals (never constexpr themselves) rely on it to
    // escape static-initialisation order.
    constexpr SmallVector() noexcept {}

    constexpr explicit SmallVector(size_type count)
    {
        resize(count);
    }

    constexpr SmallVector(size_type count, const T& value)
    {
        resize(count, value);
    }

    template<std::input_iterator InputIt>
    constexpr SmallVector(InputIt first, InputIt last)
    {
        insert(end(), first, last);
    }

    template<std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    constexpr SmallVector(std::from_range_t, R&& range)
    {
        insert_range(end(), std::forward<R>(range));
    }

    constexpr SmallVector(std::initializer_list<T> list)
    {
        insert(end(), list.begin(), list.end());
    }

    // ---- special members: trivial exactly when T's are, so SmallVector<POD, N> is trivially copyable

    SmallVector(const SmallVector&)
        requires std::is_trivially_copy_constructible_v<T>
    = default;

    constexpr SmallVector(const SmallVector& other)
        requires(!std::is_trivially_copy_constructible_v<T>)
    {
        std::uninitialized_copy(other.begin(), other.end(), data());
        size_ = other.size_;
    }

    SmallVector(SmallVector&&)
        requires std::is_trivially_move_constructible_v<T>
    = default;

    constexpr SmallVector(SmallVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires(!std::is_trivially_move_constructible_v<T>)
    {
        std::uninitialized_move(other.begin(), other.end(), data());
        size_ = other.size_;
    }

    SmallVector& operator=(const SmallVector&)
        requires kTriviallyCopyAssignable
    = default;

    constexpr SmallVector& operator=(const SmallVector& other)
        requires(!kTriviallyCopyAssignable)
    {
        if (this != &other)
        {
            AssignFrom(other.begin(), other.size_, [](const T& v) -> const T& { return v; });
        }
        return *this;
    }

    SmallVector& operator=(SmallVector&&)
        requires kTriviallyMoveAssignable
    = default;

    constexpr SmallVector& operator=(SmallVector&& other) noexcept(std::is_nothrow_move_assignable_v<T> &&
                                                                   std::is_nothrow_move_constructible_v<T>)
        requires(!kTriviallyMoveAssignable)
    {
        if (this != &other)
        {
            AssignFrom(other.begin(), other.size_, [](T& v) -> T&& { return std::move(v); });
        }
        return *this;
    }

    ~SmallVector()
        requires std::is_trivially_destructible_v<T>
    = default;

    constexpr ~SmallVector()
        requires(!std::is_trivially_destructible_v<T>)
    {
        std::destroy(begin(), end());
    }

    constexpr SmallVector& operator=(std::initializer_list<T> list)
    {
        assign(list);
        return *this;
    }

    constexpr void assign(size_type count, const T& value)
    {
        if (count > Capacity)
        {
            detail::ThrowCapacityError("assign", Capacity);
        }
        clear();
        std::uninitialized_fill_n(data(), count, value);
        size_ = count;
    }

    template<std::input_iterator InputIt>
    constexpr void assign(InputIt first, InputIt last)
    {
        clear();
        insert(end(), first, last);
    }

    constexpr void assign(std::initializer_list<T> list)
    {
        assign(list.begin(), list.end());
    }

    template<std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    constexpr void assign_range(R&& range)
    {
        clear();
        insert_range(end(), std::forward<R>(range));
    }

    // ---- iterators ---------------------------------------------------------------------------------

    [[nodiscard]] constexpr iterator begin() noexcept { return data(); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] constexpr iterator end() noexcept { return data() + size_; }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return data() + size_; }
    [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }
    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

    // ---- size and capacity -------------------------------------------------------------------------

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] static constexpr size_type max_size() noexcept { return Capacity; }
    [[nodiscard]] static constexpr size_type capacity() noexcept { return Capacity; }

    constexpr void resize(size_type count)
    {
        if (count > Capacity)
        {
            detail::ThrowCapacityError("resize", Capacity);
        }
        if (count < size_)
        {
            std::destroy(begin() + count, end());
        }
        else
        {
            std::uninitialized_value_construct(end(), begin() + count);
        }
        size_ = count;
    }

    constexpr void resize(size_type count, const T& value)
    {
        if (count > Capacity)
        {
            detail::ThrowCapacityError("resize", Capacity);
        }
        if (count < size_)
        {
            std::destroy(begin() + count, end());
        }
        else
        {
            std::uninitialized_fill(end(), begin() + count, value);
        }
        size_ = count;
    }

    static constexpr void reserve(size_type count)
    {
        if (count > Capacity)
        {
            detail::ThrowCapacityError("reserve", Capacity);
        }
    }

    static constexpr void shrink_to_fit() noexcept {}

    // ---- element access ----------------------------------------------------------------------------

    [[nodiscard]] constexpr reference operator[](size_type pos)
    {
        assert(pos < size_);
        return data()[pos];
    }

    [[nodiscard]] constexpr const_reference operator[](size_type pos) const
    {
        assert(pos < size_);
        return data()[pos];
    }

    [[nodiscard]] constexpr reference at(size_type pos)
    {
        if (pos >= size_)
        {
            throw std::out_of_range("neko::SmallVector::at: position out of range");
        }
        return data()[pos];
    }

    [[nodiscard]] constexpr const_reference at(size_type pos) const
    {
        if (pos >= size_)
        {
            throw std::out_of_range("neko::SmallVector::at: position out of range");
        }
        return data()[pos];
    }

    [[nodiscard]] constexpr reference front()
    {
        assert(size_ > 0);
        return data()[0];
    }

    [[nodiscard]] constexpr const_reference front() const
    {
        assert(size_ > 0);
        return data()[0];
    }

    [[nodiscard]] constexpr reference back()
    {
        assert(size_ > 0);
        return data()[size_ - 1];
    }

    [[nodiscard]] constexpr const_reference back() const
    {
        assert(size_ > 0);
        return data()[size_ - 1];
    }

    [[nodiscard]] constexpr T* data() noexcept { return elements_; }
    [[nodiscard]] constexpr const T* data() const noexcept { return elements_; }

    // ---- modifiers ---------------------------------------------------------------------------------

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args)
    {
        if (size_ == Capacity)
        {
            detail::ThrowCapacityError("emplace_back", Capacity);
        }
        return unchecked_emplace_back(std::forward<Args>(args)...);
    }

    constexpr reference push_back(const T& value) { return emplace_back(value); }
    constexpr reference push_back(T&& value) { return emplace_back(std::move(value)); }

    /**
     * @brief emplace_back that reports overflow instead of throwing, for callers whose capacity is a
     * data bound rather than an invariant (drop the element, warn, keep running).
     * @return the new element, or nullptr if the container was already full (nothing is constructed)
     */
    template<typename... Args>
    constexpr pointer try_emplace_back(Args&&... args)
    {
        if (size_ == Capacity)
        {
            return nullptr;
        }
        return std::addressof(unchecked_emplace_back(std::forward<Args>(args)...));
    }

    constexpr pointer try_push_back(const T& value) { return try_emplace_back(value); }
    constexpr pointer try_push_back(T&& value) { return try_emplace_back(std::move(value)); }

    template<typename... Args>
    constexpr reference unchecked_emplace_back(Args&&... args)
    {
        assert(size_ < Capacity);
        // construct_at does parenthesised aggregate initialisation (C++20), so an aggregate T takes
        // its members as arguments here just like a T with a constructor does.
        T* const slot = std::construct_at(data() + size_, std::forward<Args>(args)...);
        ++size_;
        return *slot;
    }

    constexpr reference unchecked_push_back(const T& value) { return unchecked_emplace_back(value); }
    constexpr reference unchecked_push_back(T&& value) { return unchecked_emplace_back(std::move(value)); }

    template<std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    constexpr void append_range(R&& range)
    {
        insert_range(end(), std::forward<R>(range));
    }

    /**
     * @brief Appends as many elements of range as fit.
     * @return an iterator to the first element of range that was NOT appended
     */
    template<std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    constexpr std::ranges::borrowed_iterator_t<R> try_append_range(R&& range)
    {
        auto it = std::ranges::begin(range);
        const auto last = std::ranges::end(range);
        for (; size_ < Capacity && it != last; ++it)
        {
            unchecked_emplace_back(*it);
        }
        if constexpr (std::ranges::borrowed_range<R>)
        {
            return it;
        }
        else
        {
            return std::ranges::dangling{};
        }
    }

    constexpr void pop_back()
    {
        assert(size_ > 0);
        if (size_ == 0)
        {
            return;
        }
        --size_;
        std::destroy_at(data() + size_);
    }

    template<typename... Args>
    constexpr iterator emplace(const_iterator position, Args&&... args)
    {
        const difference_type index = position - cbegin();
        // Built at the back BEFORE anything moves, so args may safely alias an element.
        emplace_back(std::forward<Args>(args)...);
        std::rotate(begin() + index, end() - 1, end());
        return begin() + index;
    }

    constexpr iterator insert(const_iterator position, const T& value) { return emplace(position, value); }
    constexpr iterator insert(const_iterator position, T&& value) { return emplace(position, std::move(value)); }

    constexpr iterator insert(const_iterator position, size_type count, const T& value)
    {
        const difference_type index = position - cbegin();
        if (count > Capacity - size_)
        {
            detail::ThrowCapacityError("insert", Capacity);
        }
        const size_type oldSize = size_;
        std::uninitialized_fill_n(end(), count, value);
        size_ += count;
        std::rotate(begin() + index, begin() + oldSize, end());
        return begin() + index;
    }

    template<std::input_iterator InputIt>
    constexpr iterator insert(const_iterator position, InputIt first, InputIt last)
    {
        return InsertAppended(position, [&] { AppendAll(first, last); });
    }

    constexpr iterator insert(const_iterator position, std::initializer_list<T> list)
    {
        return insert(position, list.begin(), list.end());
    }

    template<std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    constexpr iterator insert_range(const_iterator position, R&& range)
    {
        return InsertAppended(position,
                              [&] { AppendAll(std::ranges::begin(range), std::ranges::end(range)); });
    }

    constexpr iterator erase(const_iterator position)
    {
        assert(position >= cbegin() && position < cend());
        return erase(position, position + 1);
    }

    constexpr iterator erase(const_iterator first, const_iterator last)
    {
        const iterator from = begin() + (first - cbegin());
        const iterator to = begin() + (last - cbegin());
        if (from != to)
        {
            const iterator newEnd = std::move(to, end(), from);
            std::destroy(newEnd, end());
            size_ -= static_cast<size_type>(to - from);
        }
        return from;
    }

    constexpr void clear() noexcept
    {
        std::destroy(begin(), end());
        size_ = 0;
    }

    constexpr void swap(SmallVector& other) noexcept(std::is_nothrow_swappable_v<T> &&
                                                     std::is_nothrow_move_constructible_v<T>)
    {
        SmallVector* shorter = this;
        SmallVector* longer = &other;
        if (shorter->size_ > longer->size_)
        {
            std::swap(shorter, longer);
        }
        std::swap_ranges(shorter->begin(), shorter->end(), longer->begin());
        const iterator tail = longer->begin() + shorter->size_;
        std::uninitialized_move(tail, longer->end(), shorter->end());
        std::destroy(tail, longer->end());
        std::swap(shorter->size_, longer->size_);
    }

    friend constexpr void swap(SmallVector& lhs, SmallVector& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    // ---- comparison --------------------------------------------------------------------------------

    [[nodiscard]] friend constexpr bool operator==(const SmallVector& lhs, const SmallVector& rhs)
        requires std::equality_comparable<T>
    {
        return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

    [[nodiscard]] friend constexpr auto operator<=>(const SmallVector& lhs, const SmallVector& rhs)
        requires std::three_way_comparable<T>
    {
        return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }

private:
    // Copy/move assignment from count elements starting at source: assign over the common prefix (so a
    // std::string element keeps its buffer), then construct the surplus or destroy the excess.
    template<typename Source, typename Forward>
    constexpr void AssignFrom(Source source, size_type count, Forward forward)
    {
        const size_type common = std::min(size_, count);
        for (size_type i = 0; i < common; i++)
        {
            data()[i] = forward(source[i]);
        }
        if (count < size_)
        {
            std::destroy(begin() + count, end());
            size_ = count;
        }
        else
        {
            for (size_type i = common; i < count; i++)
            {
                unchecked_emplace_back(forward(source[i]));
            }
        }
    }

    template<typename It, typename Sentinel>
    constexpr void AppendAll(It first, Sentinel last)
    {
        if constexpr (std::sized_sentinel_for<Sentinel, It>)
        {
            // Sized: refuse before constructing anything, so an overflowing insert changes nothing.
            if (static_cast<size_type>(last - first) > Capacity - size_)
            {
                detail::ThrowCapacityError("insert", Capacity);
            }
        }
        for (; first != last; ++first)
        {
            emplace_back(*first);
        }
    }

    // Runs append (which pushes at the back), then rotates the appended block into place. If append
    // throws part-way -- an unsized range running out of room, or a throwing T constructor -- the
    // partial tail is destroyed before rethrowing, so no half-inserted range survives.
    template<typename Append>
    constexpr iterator InsertAppended(const_iterator position, Append append)
    {
        const difference_type index = position - cbegin();
        const size_type oldSize = size_;
        try
        {
            append();
        }
        catch (...)
        {
            std::destroy(begin() + oldSize, end());
            size_ = oldSize;
            throw;
        }
        std::rotate(begin() + index, begin() + oldSize, end());
        return begin() + index;
    }

    size_type size_ = 0;
    union
    {
        T elements_[Capacity == 0 ? 1 : Capacity];
    };
};

template<typename T, std::size_t Capacity, typename U = T>
constexpr typename SmallVector<T, Capacity>::size_type erase(SmallVector<T, Capacity>& container, const U& value)
{
    const auto newEnd = std::remove(container.begin(), container.end(), value);
    const auto removed = static_cast<std::size_t>(container.end() - newEnd);
    container.erase(newEnd, container.end());
    return removed;
}

template<typename T, std::size_t Capacity, typename Predicate>
constexpr typename SmallVector<T, Capacity>::size_type erase_if(SmallVector<T, Capacity>& container,
                                                                Predicate predicate)
{
    const auto newEnd = std::remove_if(container.begin(), container.end(), predicate);
    const auto removed = static_cast<std::size_t>(container.end() - newEnd);
    container.erase(newEnd, container.end());
    return removed;
}

template<typename T, std::size_t Capacity, class Allocator = std::allocator<T>>
class FixedVector
{
public:
    constexpr FixedVector()
    {
        underlyingContainer_.reserve(Capacity);
    }

    constexpr FixedVector(const Allocator& allocator): underlyingContainer_(allocator)
    {
        underlyingContainer_.reserve(Capacity);
    }

    constexpr FixedVector(std::initializer_list<T> list)
    {
        if(list.size() > Capacity)
        {
            throw std::out_of_range("Over capacity");
        }
        underlyingContainer_.reserve(Capacity);
        underlyingContainer_ = list;
    }

    constexpr auto begin()
    {
        return underlyingContainer_.begin();
    }

    constexpr auto end()
    {
        return underlyingContainer_.end();
    }

    constexpr auto cbegin()
    {
        return underlyingContainer_.cbegin();
    }

    constexpr auto cend()
    {
        return underlyingContainer_.cend();
    }

    constexpr void push_back( const T& value )
    {
        if(underlyingContainer_.size() == underlyingContainer_.capacity())
        {
            throw std::out_of_range("Over capacity");
        }
        underlyingContainer_.push_back(value);
    }

    constexpr void push_back(T&& value)
    {
        if(underlyingContainer_.size() == underlyingContainer_.capacity())
        {
            throw std::out_of_range("Over capacity");
        }
        underlyingContainer_.push_back(std::move(value));
    }

    constexpr void clear()
    {
        underlyingContainer_.clear();
    }

    constexpr T& operator[]( std::size_t pos )
    {
        return underlyingContainer_[pos];
    }

    constexpr const T& operator[]( std::size_t pos ) const
    {
        return underlyingContainer_[pos];
    }
    constexpr auto insert(typename std::vector<T>::const_iterator pos, const T& value )
    {
        if(underlyingContainer_.size() == underlyingContainer_.capacity())
        {
            throw std::out_of_range("Over capacity");
        }
        return underlyingContainer_.insert(pos, value);
    }

    constexpr auto insert(typename std::vector<T>::const_iterator pos, T&& value )
    {
        if(underlyingContainer_.size() == underlyingContainer_.capacity())
        {
            throw std::out_of_range("Over capacity");
        }
        return underlyingContainer_.insert(pos, std::move(value));
    }
    constexpr auto erase(typename  std::vector<T>::iterator pos )
    {
        return underlyingContainer_.erase(pos);
    }
    constexpr auto erase(typename  std::vector<T>::const_iterator pos )
    {
        return underlyingContainer_.erase(pos);
    }

    constexpr auto capacity() const
    {
        if(underlyingContainer_.capacity() != Capacity)
        {
            //Bug with different capacity
            std::terminate();
        }
        return underlyingContainer_.capacity();
    }
    constexpr auto size() const
    {
        return underlyingContainer_.size();
    }

    constexpr T& front()
    {
        return underlyingContainer_.front();
    }
    constexpr const T& front() const
    {
        return underlyingContainer_.front();
    }
    constexpr auto data() noexcept
    {
        return underlyingContainer_.data();
    }
private:
    std::vector<T, Allocator> underlyingContainer_;
};
//TODO add a new type like std::hive, allocating blocks
//How does it work for insert/erase?
}

#endif //NEKOLIB_FIXED_VECTOR_H
