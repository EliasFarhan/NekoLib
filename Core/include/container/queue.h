//
// Created by unite on 05.06.2024.
//

#ifndef NEKOLIB_QUEUE_H
#define NEKOLIB_QUEUE_H

#include <vector>
#include <exception>
#include <array>
#include <algorithm>

namespace neko
{
template<typename T>
class Queue
{
public:
    constexpr Queue() = default;
    constexpr Queue(const std::initializer_list<T> list): underlyingContainer_(list)
    {
        size_ = list.size();
    }
    constexpr void reserve(std::size_t newCapacity)
    {
        underlyingContainer_.reserve(newCapacity);
    }
    constexpr void push(const T& value)
    {
        if(underlyingContainer_.size() == size())
        {
            if (front_ != 0)
            {
                Rearrange();
            }
            underlyingContainer_.push_back(value);
            size_++;
        }
        else
        {
            underlyingContainer_[(front_ + size_) % underlyingContainer_.size()] = value;
            size_++;
        }
    }

    constexpr void pop()
    {
        if(size_ == 0)
        {
            std::terminate();
        }
        underlyingContainer_[front_] = {};
        front_ = (front_+1) % underlyingContainer_.size();
        size_--;
    }

    [[nodiscard]] constexpr T& front()
    {
        return underlyingContainer_[front_];
    }
    [[nodiscard]] constexpr const T& front() const
    {
        return underlyingContainer_[front_];
    }
    [[nodiscard]] constexpr T& back()
    {
        return underlyingContainer_[(front_+size_-1)%underlyingContainer_.size()];
    }
    [[nodiscard]] constexpr const T& back() const
    {
        return underlyingContainer_[(front_+size_-1)%underlyingContainer_.size()];
    }
    [[nodiscard]] constexpr auto size() const
    {
        return size_;
    }
    [[nodiscard]] constexpr auto capacity() const
    {
        return underlyingContainer_.capacity();
    }

    [[nodiscard]] constexpr auto data()
    {
        return underlyingContainer_.data();
    }
private:
    void Rearrange()
    {
        std::rotate(underlyingContainer_.begin(), underlyingContainer_.begin()+front_, underlyingContainer_.end());
        front_ = 0;
    }
    std::vector<T> underlyingContainer_;
    std::size_t front_ = 0, size_ = 0;
};

template<typename T, std::size_t Capacity>
class FixedQueue
{
public:
    constexpr void push(const T& value)
    {
        if(underlyingContainer_.size() == size())
        {
            std::terminate();
        }
        underlyingContainer_[(front_ + size_) % Capacity] = value;
        size_++;
    }

    constexpr void push(T&& value)
    {
        if(underlyingContainer_.size() == size())
        {
            std::terminate();
        }
        underlyingContainer_[(front_ + size_) % Capacity] = std::move(value);
        size_++;
    }

    constexpr void pop()
    {
        if(size_ == 0)
        {
            std::terminate();
        }
        underlyingContainer_[front_] = {};
        front_ = (front_+1) % underlyingContainer_.size();
        size_--;
    }

    [[nodiscard]] constexpr T& front()
    {
        return underlyingContainer_[front_];
    }
    [[nodiscard]] constexpr const T& front() const
    {
        return underlyingContainer_[front_];
    }
    [[nodiscard]] constexpr T& back()
    {
        return underlyingContainer_[(front_+size_-1)%Capacity];
    }
    [[nodiscard]] constexpr const T& back() const
    {
        return underlyingContainer_[(front_+size_-1)%Capacity];
    }
    [[nodiscard]] constexpr auto size() const
    {
        return size_;
    }
    [[nodiscard]] constexpr auto capacity() const
    {
        return Capacity;
    }

    [[nodiscard]] constexpr auto data()
    {
        return underlyingContainer_.data();
    }
private:
    std::array<T, Capacity> underlyingContainer_;
    std::size_t front_ = 0, size_ = 0;
};
}

#endif //NEKOLIB_QUEUE_H
