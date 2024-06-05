//
// Created by unite on 05.06.2024.
//

#ifndef NEKOLIB_FIXED_VECTOR_H
#define NEKOLIB_FIXED_VECTOR_H

#include <vector>

namespace neko
{

    template<typename T, std::size_t Capacity>
    class FixedVector
    {
    public:
        FixedVector()
        {
            data.reserve(Capacity);
        }

        FixedVector(std::initializer_list<T> list)
        {
            if(list.size() > Capacity)
            {
                // Over capacity construction
                std::terminate();
            }
            data.reserve(Capacity);
            data = list;
        }

        auto begin()
        {
            return data.begin();
        }

        auto end()
        {
            return data.end();
        }

        auto cbegin()
        {
            return data.cbegin();
        }

        auto cend()
        {
            return data.cend();
        }

        void push_back( const T& value )
        {
            if(data.size() == data.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            data.push_back(value);
        }

        void push_back(T&& value)
        {
            if(data.size() == data.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            data.push_back(std::move(value));
        }

        void clear()
        {
            data.clear();
        }

        T& operator[]( std::size_t pos )
        {
            return data[pos];
        }

        const T& operator[]( std::size_t pos ) const
        {
            return data[pos];
        }
        auto insert(std::vector<T>::const_iterator pos, const T& value )
        {
            if(data.size() == data.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            return data.insert(pos, value);
        }

        auto insert(std::vector<T>::const_iterator pos, T&& value )
        {
            if(data.size() == data.capacity())
            {
                // Over-capacity leads to a crash
                std::terminate();
            }
            return data.insert(pos, std::move(value));
        }
        auto erase( std::vector<T>::iterator pos )
        {
            return data.erase(pos);
        }
        auto erase( std::vector<T>::const_iterator pos )
        {
            return data.erase(pos);
        }

        constexpr auto capacity() const
        {
            if(data.capacity() != Capacity)
            {
                //Bug with different capacity
                std::terminate();
            }
            return data.capacity();
        }
        constexpr auto size() const
        {
            return data.size();
        }
    private:
        std::vector<T> data;
    };
}

#endif //NEKOLIB_FIXED_VECTOR_H
