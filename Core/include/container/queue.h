//
// Created by unite on 05.06.2024.
//

#ifndef NEKOLIB_QUEUE_H
#define NEKOLIB_QUEUE_H

#include <vector>

namespace neko
{
template<typename T>
class Queue
{
public:
    void Push(const T& value)
    {
        if(underlyingContainer_.size() == size())
        {
            if (front_ != 0)
            {
                Rearrange();
            }
            underlyingContainer_.push_back(value);
            back_++;
        }
        else
        {
            auto newBack = (back_ + 1)%underlyingContainer_.size();
            underlyingContainer_[newBack] = value;
            back_ = newBack;
        }
    }

    void Pop()
    {
        underlyingContainer_[front_] = {};
        front_ = (front_+1) % underlyingContainer_.size();
    }

    T& front()
    {
        return underlyingContainer_[front_];
    }
    const T& front() const
    {
        return underlyingContainer_[front_];
    }
    T& back()
    {
        return underlyingContainer_[back_];
    }
    const T& back() const
    {
        return underlyingContainer_[back_];
    }
    [[nodiscard]] auto size() const
    {
        auto result = back_-front_;
        if(result < 0)
        {
            result = (std::int64_t )underlyingContainer_.size()-result-1;
        }
        return (std::size_t)result;
    }
    [[nodiscard]] auto capacity() const
    {
        return underlyingContainer_.capacity();
    }

    auto data()
    {
        return underlyingContainer_.data();
    }
private:
    void Rearrange()
    {
        std::rotate(underlyingContainer_.begin(), underlyingContainer_.begin()+front_, underlyingContainer_.end());
        front_ = 0;
        back_ = underlyingContainer_.size()-1;
    }
    std::vector<T> underlyingContainer_;
    std::int64_t front_ = 0, back_ = 0;
};
}

#endif //NEKOLIB_QUEUE_H
