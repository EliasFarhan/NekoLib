//
// Created by unite on 05.06.2024.
//

#ifndef NEKOLIB_FIXED_VECTOR_H
#define NEKOLIB_FIXED_VECTOR_H

#include <vector>
#include <variant>
#include <array>
#include <iterator>

namespace neko
{

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
                std::terminate();
            }
            std::copy(list.begin(), list.end(), underlyingContainer_.begin());
            size_ = size;
        }

        constexpr auto begin()
        {
            return underlyingContainer_.begin();
        }

        constexpr auto end()
        {
            return underlyingContainer_.begin()+size_;
        }

        constexpr auto cbegin() const
        {
            return underlyingContainer_.cbegin();
        }

        constexpr auto cend() const
        {
            return underlyingContainer_.cbegin()+size_;
        }

        constexpr void push_back( const T& value )
        {
            if(size_ == Capacity)
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            underlyingContainer_[size_] = value;
            size_++;
        }

        constexpr void push_back(T&& value)
        {
            if(size_ == Capacity)
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            underlyingContainer_[size_] = std::move(value);
            size_++;
        }

        constexpr void clear()
        {
            if constexpr (std::is_destructible_v<T>)
            {
                for(int i = 0; i < size_; i++)
                {
                    underlyingContainer_[i].~T();
                }
            }
            size_ = 0;
        }

        constexpr void resize(std::size_t newSize)
        {
            if(size_ == newSize)
            {
                return;
            }
            if(size_ < newSize)
            {
                for (auto i = size_; i < newSize; i++)
                {
                    underlyingContainer_[i] = {};
                }
            }
            else
            {
                if constexpr (std::is_destructible_v<T>)
                {
                    for (auto i = newSize; i < size_; i++)
                    {
                        underlyingContainer_[i].~T();
                    }
                }
            }
            size_ = newSize;
        }

        constexpr T& operator[]( std::size_t pos )
        {
            return underlyingContainer_[pos];
        }

        constexpr const T& operator[]( std::size_t pos ) const
        {
            return underlyingContainer_[pos];
        }

        constexpr typename std::array<T, Capacity>::const_iterator insert(typename std::array<T, Capacity>::const_iterator pos, const T& value )
        {
            if(size_ == Capacity)
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            const auto index = std::distance(cbegin(), pos);
            for(auto i = size_; i > index; i--)
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
                // Over-capacity leads to a crash
                std::terminate();
            }
            const auto index = std::distance(cbegin(), pos);
            for(auto i = size_; i > index; i--)
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
            const auto index = std::distance(cbegin(), pos);
            for(auto i = index; i < size_-1; i++)
            {
                underlyingContainer_[i] = std::move(underlyingContainer_[i+1]);
            }
            size_--;
            return pos;
        }

        constexpr auto capacity() const
        {
            return Capacity;
        }
        constexpr auto size() const
        {
            return size_;
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
                // Over capacity construction
                std::terminate();
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
                // Over-capacity leads to a crash
                std::terminate();
            }
            underlyingContainer_.push_back(value);
        }

        constexpr void push_back(T&& value)
        {
            if(underlyingContainer_.size() == underlyingContainer_.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
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
                // Over-capacity leads to a crash
                std::terminate();
            }
            return underlyingContainer_.insert(pos, value);
        }

        constexpr auto insert(typename std::vector<T>::const_iterator pos, T&& value )
        {
            if(underlyingContainer_.size() == underlyingContainer_.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
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
        constexpr BasicVector() : allocator_(std::allocator<T>())
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
        auto insert(Iterator pos, const T& value )
        {
        }

        auto insert(Iterator pos, T&& value )
        {
        }
        auto erase(Iterator pos )
        {
        }

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
        Allocator allocator_;
        std::size_t size_ = 0;
    };
}

#endif //NEKOLIB_FIXED_VECTOR_H
