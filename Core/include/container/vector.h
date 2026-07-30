//
// Created by unite on 05.06.2024.
//

#ifndef NEKOLIB_FIXED_VECTOR_H
#define NEKOLIB_FIXED_VECTOR_H

#include <vector>
#include <variant>
#include <array>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace neko
{
/**
 * @brief SmallVector is a vector-like on stack fixed-sized container that allows to work like std::vector but on the stack
 *
 * Storage invariant: ALL Capacity slots of the backing std::array are live, valid T objects for the
 * whole lifetime of the container. size_ is the logical size only -- it does not bound object
 * lifetime. push_back/emplace_back therefore ASSIGN into an already-live slot rather than
 * constructing into raw storage, and clear()/pop_back()/shrinking resize() must NOT call ~T():
 * the backing std::array destroys every slot itself at end of scope, so an explicit destructor call
 * here would leave a destroyed object that the next push_back assigns to (use-after-destroy) and
 * that the array then destroys a second time (double free). For a T that owns resources, the
 * logically-removed slots are assigned a fresh T{} instead, which releases those resources at the
 * same point an explicit destructor call was intended to.
 *
 * Consequences worth knowing before choosing this container:
 * - T must be default-constructible and assignable, and Capacity objects are constructed up front.
 * - Copying a SmallVector copies all Capacity slots, not just the first size_ of them.
 * - push_back/emplace_back/insert/resize THROW std::out_of_range past Capacity. Use try_push_back /
 *   try_emplace_back where overflow is a data condition to handle rather than a programming error.
 * @tparam T
 * @tparam Capacity
 */
template<typename T, std::size_t Capacity>
    class SmallVector
    {
    public:
        constexpr SmallVector() = default;

        constexpr SmallVector(std::initializer_list<T> list)
        {
            const auto size = list.size();
            if(size > Capacity)
            {
                throw std::out_of_range("Error: trying to insert with size over capacity");
            }
            std::copy(list.begin(), list.end(), underlyingContainer_.begin());
            size_ = size;
        }

        [[nodiscard]] constexpr auto begin()
        {
            return underlyingContainer_.begin();
        }

        [[nodiscard]] constexpr auto end()
        {
            return underlyingContainer_.begin()+size_;
        }

		[[nodiscard]] constexpr auto begin() const
		{
			return underlyingContainer_.begin();
		}

		[[nodiscard]] constexpr auto end() const
		{
			return underlyingContainer_.begin()+size_;
		}

        [[nodiscard]] constexpr auto cbegin() const
        {
            return underlyingContainer_.cbegin();
        }

        [[nodiscard]] constexpr auto cend() const
        {
            return underlyingContainer_.cbegin()+size_;
        }

        constexpr void push_back( const T& value )
        {
            if(size_ == Capacity)
            {
                throw std::out_of_range("Error: trying to push_back with size over capacity");
            }
            underlyingContainer_[size_] = value;
            size_++;
        }

        constexpr void push_back(T&& value)
        {
            if(size_ == Capacity)
            {
                throw std::out_of_range("Error: trying to push_back with size over capacity");
            }
            underlyingContainer_[size_] = std::move(value);
            size_++;
        }

        /**
         * @brief push_back that reports overflow instead of throwing, for callers whose capacity is a
         * data bound rather than an invariant (drop the element, warn, keep running).
         * @return true if the value was appended, false if the container was already full
         */
        [[nodiscard]] constexpr bool try_push_back(const T& value)
        {
            if(size_ == Capacity)
            {
                return false;
            }
            underlyingContainer_[size_] = value;
            size_++;
            return true;
        }

        [[nodiscard]] constexpr bool try_push_back(T&& value)
        {
            if(size_ == Capacity)
            {
                return false;
            }
            underlyingContainer_[size_] = std::move(value);
            size_++;
            return true;
        }

        /**
         * @brief Assigns a T built from args into the next slot. NOT in-place construction -- the slot
         * is already a live object per the storage invariant, so this is an assignment from a
         * temporary and cannot be used to hold a non-assignable T.
         */
        template<typename... Args>
        constexpr T& emplace_back(Args&&... args)
        {
            if(size_ == Capacity)
            {
                throw std::out_of_range("Error: trying to emplace_back with size over capacity");
            }
            if constexpr (std::is_constructible_v<T, Args...>)
            {
                underlyingContainer_[size_] = T(std::forward<Args>(args)...);
            }
            else
            {
                underlyingContainer_[size_] = T{std::forward<Args>(args)...};
            }
            size_++;
            return underlyingContainer_[size_-1];
        }

        template<typename... Args>
        [[nodiscard]] constexpr bool try_emplace_back(Args&&... args)
        {
            if(size_ == Capacity)
            {
                return false;
            }
            if constexpr (std::is_constructible_v<T, Args...>)
            {
                underlyingContainer_[size_] = T(std::forward<Args>(args)...);
            }
            else
            {
                underlyingContainer_[size_] = T{std::forward<Args>(args)...};
            }
            size_++;
            return true;
        }

        constexpr void pop_back()
        {
            if(size_ == 0)
            {
                return;
            }
            size_--;
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                underlyingContainer_[size_] = T{};
            }
        }

        /**
         * @brief Logically empties the container. Does not call ~T() -- see the storage invariant.
         * A resource-owning T has its live slots assigned a fresh T{}, which releases them here.
         */
        constexpr void clear()
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for(std::size_t i = 0; i < size_; i++)
                {
                    underlyingContainer_[i] = T{};
                }
            }
            size_ = 0;
        }

        constexpr void resize(std::size_t newSize, T newValue={})
        {
            // Unchecked in the original: a resize past Capacity wrote straight off the end of the
            // backing array. Every other growth path here throws, so this one does too.
            if(newSize > Capacity)
            {
                throw std::out_of_range("Error: trying to resize with size over capacity");
            }
            if(size_ == newSize)
            {
                return;
            }
            if(size_ < newSize)
            {
                for (auto i = size_; i < newSize; i++)
                {
                    underlyingContainer_[i] = newValue;
                }
            }
            else if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for (auto i = newSize; i < size_; i++)
                {
                    underlyingContainer_[i] = T{};
                }
            }
            size_ = newSize;
        }

        [[nodiscard]] constexpr T& at (std::size_t pos)
        {
            if (pos >= size_)
            {
                throw std::out_of_range("Error: trying to access at position " + std::to_string(pos));
            }
            return underlyingContainer_[pos];
        }
        [[nodiscard]] constexpr const T& at (std::size_t pos) const
        {
            if (pos >= size_)
            {
                throw std::out_of_range("Error: trying to access at position " + std::to_string(pos));
            }
            return underlyingContainer_[pos];
        }
        [[nodiscard]] constexpr T& operator[]( std::size_t pos ) noexcept
        {
            return underlyingContainer_[pos];
        }

        [[nodiscard]] constexpr const T& operator[]( std::size_t pos ) const noexcept
        {
            return underlyingContainer_[pos];
        }

        constexpr typename std::array<T, Capacity>::const_iterator insert(typename std::array<T, Capacity>::const_iterator pos, const T& value )
        {
            if(size_ == Capacity)
            {
                throw std::out_of_range("Error: trying to insert with size over capacity");
            }
            const auto index = std::distance(cbegin(), pos);
            for(auto i = static_cast<std::ptrdiff_t>(size_); i > index; i--)
            {
                underlyingContainer_[i] = std::move(underlyingContainer_[i-1]);
            }
            underlyingContainer_[index] = value;
            size_++;
            return pos;
        }

        constexpr typename std::array<T, Capacity>::const_iterator insert(typename std::array<T, Capacity>::const_iterator pos, T&& value )
        {
            if(size_ == Capacity)
            {
                throw std::out_of_range("Error: trying to insert with size over capacity");
            }
            const auto index = std::distance(cbegin(), pos);
            for(auto i = static_cast<std::ptrdiff_t>(size_); i > index; i--)
            {
                underlyingContainer_[i] = std::move(underlyingContainer_[i-1]);
            }
            underlyingContainer_[index] = std::move(value);
            size_++;
            return pos;
        }

        constexpr auto erase(typename  std::array<T, Capacity>::iterator pos )
        {
            std::move(pos+1, end(), pos);
            size_--;
            return pos;
        }

        constexpr auto erase(typename  std::array<T, Capacity>::const_iterator pos )
        {
            const auto index = static_cast<std::size_t>(std::distance(cbegin(), pos));
            for(auto i = index; i < size_-1; i++)
            {
                underlyingContainer_[i] = std::move(underlyingContainer_[i+1]);
            }
            size_--;
            return pos;
        }

        /**
         * @brief Range erase, so the erase-remove idiom works:
         * v.erase(std::remove_if(v.begin(), v.end(), pred), v.end())
         * The vacated tail slots stay live objects per the storage invariant; a resource-owning T has
         * them released.
         */
        constexpr auto erase(typename std::array<T, Capacity>::iterator first,
                             typename std::array<T, Capacity>::iterator last )
        {
            if(first == last)
            {
                return first;
            }
            const auto out = std::move(last, end(), first);
            const auto removed = static_cast<std::size_t>(std::distance(first, last));
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                for(auto it = out; it != end(); ++it)
                {
                    *it = T{};
                }
            }
            size_ -= removed;
            return first;
        }

        [[nodiscard]] static constexpr auto capacity() noexcept
        {
            return Capacity;
        }

        [[nodiscard]] constexpr auto size() const
        {
            return size_;
        }

        [[nodiscard]] constexpr T& front()
        {
            return underlyingContainer_.front();
        }
        [[nodiscard]] constexpr const T& front() const
        {
            return underlyingContainer_.front();
        }
        [[nodiscard]] constexpr T& back()
        {
            return underlyingContainer_[size_-1];
        }
        [[nodiscard]] constexpr const T& back() const
        {
            return underlyingContainer_[size_-1];
        }
        [[nodiscard]] constexpr auto data() noexcept
        {
            return underlyingContainer_.data();
        }
        [[nodiscard]] constexpr auto data() const noexcept
        {
            return underlyingContainer_.data();
        }
		[[nodiscard]] constexpr bool is_full() const {return size_ == Capacity;}
		[[nodiscard]] constexpr bool is_empty() const { return size_ == 0;}
		// std::vector-compatible spelling, so a converted call site keeps reading the same.
		[[nodiscard]] constexpr bool empty() const { return size_ == 0;}

		[[nodiscard]] bool operator==(const SmallVector& other) const
		{
			return size_ == other.size_ && underlyingContainer_ == other.underlyingContainer_;
		}
		[[nodiscard]] bool operator!=(const SmallVector& other) const
		{
			return !operator==(other);
		}
    private:
        std::array<T, Capacity> underlyingContainer_{};
        std::size_t size_ = 0;
    };

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

    template<typename T, std::size_t Capacity, class Allocator = std::allocator<T>>
    class BasicVector
    {
    public:
        constexpr BasicVector() : allocator_(Allocator())
        {
            underlyingContainer_ = SmallVector<T, Capacity>();
        }

        constexpr BasicVector(const Allocator& allocator): allocator_(allocator)
        {
            underlyingContainer_ = SmallVector<T, Capacity>();
        }

        constexpr BasicVector(std::initializer_list<T> list)
        {
            size_ = list.size();
            if(size_ > Capacity)
            {
                underlyingContainer_ = std::move(std::vector<T, Allocator>(list));
            }
            else
            {
                underlyingContainer_ = SmallVector<T, Capacity>(list);
            }
        }

        class Iterator
        {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using difference_type   = std::ptrdiff_t;
            using value_type        = T;
            using pointer           = T*;  // or also value_type*
            using reference         = T&;  // or also value_type&

            constexpr Iterator(pointer ptr) : ptr_(ptr){}

            constexpr reference operator*() const { return *ptr_; }
            constexpr pointer operator->() { return ptr_; }

            // Prefix increment
            constexpr Iterator& operator++() { ptr_++; return *this; }

            // Postfix increment
            constexpr Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

            constexpr friend bool operator== (const Iterator& a, const Iterator& b) { return a.ptr_ == b.ptr_; };
            constexpr friend bool operator!= (const Iterator& a, const Iterator& b) { return a.ptr_ != b.ptr_; };
        private:
            pointer ptr_ = nullptr;
        };

        constexpr Iterator begin()
        {
            switch(underlyingContainer_.index())
            {
            case 0:
                return Iterator(std::get<0>(underlyingContainer_).data());
            case 1:
                return Iterator(std::get<1>(underlyingContainer_).data());
            default:
                std::terminate();
            }
        }

        constexpr Iterator end()
        {
            switch(underlyingContainer_.index())
            {
            case 0:
            {
                auto &array = std::get<0>(underlyingContainer_);
                return Iterator(array.data() + size_);
            }
            case 1:
            {
                auto &vector = std::get<1>(underlyingContainer_);
                return Iterator(vector.data() + size_);
            }
            default:
                std::terminate();
            }
        }

        void resize(std::size_t newSize)
        {
            if(newSize > Capacity)
            {
                SwitchToHeap();
                auto& vector = std::get<1>(underlyingContainer_);
                vector.resize(newSize);
                size_ = vector.size();
            }
            else
            {
                auto &array = std::get<0>(underlyingContainer_);
                array.resize(newSize);
                size_ = array.size();
            }
        }

        void push_back( const T& value )
        {
            if(size_ >= Capacity)
            {
                SwitchToHeap();
            }
            switch(underlyingContainer_.index())
            {
            case 0:
            {
                auto& array = std::get<0>(underlyingContainer_);
                array[size_] = value;
                break;
            }
            case 1:
                std::get<1>(underlyingContainer_).push_back(value);
                break;
            default:
                std::terminate();
            }
            size_++;
        }

        void push_back(T&& value)
        {
            if(size_ >= Capacity)
            {
                SwitchToHeap();
            }
            switch(underlyingContainer_.index())
            {
            case 0:
            {
                auto& array = std::get<0>(underlyingContainer_);
                array[size_] = std::move(value);
                break;
            }
            case 1:
                std::get<1>(underlyingContainer_).push_back(std::move(value));
                break;
            default:
                std::terminate();
            }
            size_++;
        }

        void clear()
        {
            if(underlyingContainer_.index() == 1)
            {
                std::get<1>(underlyingContainer_).clear();
                underlyingContainer_ = SmallVector<T, Capacity>();
                size_ = 0;
                return;
            }

            if constexpr (std::is_destructible_v<T>)
            {
                for(auto it = begin(); it != end(); it++)
                {
                    (*it).~T();
                }
            }
            size_ = 0;
        }

        T& operator[]( std::size_t pos )
        {
            switch(underlyingContainer_.index())
            {
            case 0:
                return std::get<0>(underlyingContainer_)[pos];
            case 1:
                return std::get<1>(underlyingContainer_)[pos];
            default:
                std::terminate();
            }
        }

        const T& operator[]( std::size_t pos ) const
        {
            switch(underlyingContainer_.index())
            {
            case 0:
                return std::get<0>(underlyingContainer_)[pos];
            case 1:
                return std::get<1>(underlyingContainer_)[pos];
            default:
                std::terminate();
            }
        }
		/*
        auto insert(Iterator pos, const T& value )
        {
        }

        auto insert(Iterator pos, T&& value )
        {
        }
        auto erase(Iterator pos )
        {
        }
		*/
        constexpr auto capacity() const
        {
            if(underlyingContainer_.index() == 0)
            {
                return Capacity;
            }
            else
            {
                return std::get<1>(underlyingContainer_).capacity();
            }
        }
        constexpr auto size() const
        {
            return size_;
        }

        T& front()
        {
            switch(underlyingContainer_.index())
            {
            case 0:
            {
                return std::get<0>(underlyingContainer_)[0];
            }
            case 1:
                return std::get<1>(underlyingContainer_)[0];
            default:
                std::terminate();
            }
        }
        const T& front() const
        {
            switch(underlyingContainer_.index())
            {
            case 0:
            {
                return std::get<0>(underlyingContainer_)[0];
            }
            case 1:
                return std::get<1>(underlyingContainer_)[0];
            default:
                std::terminate();
            }
        }
        auto data() noexcept
        {
            return underlyingContainer_.data();
        }
    private:
        void SwitchToHeap()
        {
            if(underlyingContainer_.index() != 0)
                return;
            auto& array = std::get<0>(underlyingContainer_);
            std::vector<T, Allocator> v(array.begin(), array.end(), allocator_);
            underlyingContainer_ = std::move(v);
        }
        std::variant<SmallVector<T, Capacity>, std::vector<T, Allocator>> underlyingContainer_;
        std::size_t size_ = 0;
        [[no_unique_address]] Allocator allocator_;
    };
//TODO add a new type like std::hive, allocating blocks
//How does it work for insert/erase?
}

#endif //NEKOLIB_FIXED_VECTOR_H
