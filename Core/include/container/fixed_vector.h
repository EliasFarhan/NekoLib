//
// Created by unite on 05.06.2024.
//

#ifndef NEKOLIB_FIXED_VECTOR_H
#define NEKOLIB_FIXED_VECTOR_H

#include <vector>

namespace neko
{

    template<typename T, std::size_t Capacity, class Allocator = std::allocator<T>>
    class FixedVector
    {
    public:
        FixedVector()
        {
            underlyingContainer_.reserve(Capacity);
        }

        FixedVector(const Allocator& allocator): underlyingContainer_(allocator)
        {
            underlyingContainer_.reserve(Capacity);
        }

        FixedVector(std::initializer_list<T> list)
        {
            if(list.size() > Capacity)
            {
                // Over capacity construction
                std::terminate();
            }
            underlyingContainer_.reserve(Capacity);
            underlyingContainer_ = list;
        }

        auto begin()
        {
            return underlyingContainer_.begin();
        }

        auto end()
        {
            return underlyingContainer_.end();
        }

        auto cbegin()
        {
            return underlyingContainer_.cbegin();
        }

        auto cend()
        {
            return underlyingContainer_.cend();
        }

        void push_back( const T& value )
        {
            if(underlyingContainer_.size() == underlyingContainer_.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            underlyingContainer_.push_back(value);
        }

        void push_back(T&& value)
        {
            if(underlyingContainer_.size() == underlyingContainer_.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            underlyingContainer_.push_back(std::move(value));
        }

        void clear()
        {
            underlyingContainer_.clear();
        }

        T& operator[]( std::size_t pos )
        {
            return underlyingContainer_[pos];
        }

        const T& operator[]( std::size_t pos ) const
        {
            return underlyingContainer_[pos];
        }
        auto insert(std::vector<T>::const_iterator pos, const T& value )
        {
            if(underlyingContainer_.size() == underlyingContainer_.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            return underlyingContainer_.insert(pos, value);
        }

        auto insert(std::vector<T>::const_iterator pos, T&& value )
        {
            if(underlyingContainer_.size() == underlyingContainer_.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            return underlyingContainer_.insert(pos, std::move(value));
        }
        auto erase( std::vector<T>::iterator pos )
        {
            return underlyingContainer_.erase(pos);
        }
        auto erase( std::vector<T>::const_iterator pos )
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

        T& front()
        {
            return underlyingContainer_.front();
        }
        const T& front() const
        {
            return underlyingContainer_.front();
        }
        auto data() noexcept
        {
            return underlyingContainer_.data();
        }
    private:
        std::vector<T, Allocator> underlyingContainer_;
    };
}

#endif //NEKOLIB_FIXED_VECTOR_H
