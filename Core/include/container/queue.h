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
            size_++;
        }
        else
        {
            underlyingContainer_[(front_ + size_) % underlyingContainer_.size()] = value;
            size_++;
        }
    }

    void Pop()
    {
        if(size_ == 0)
        {
            std::terminate();
        }
        underlyingContainer_[front_] = {};
        front_ = (front_+1) % underlyingContainer_.size();
        size_--;
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
        return underlyingContainer_[(front_+size_-1)%underlyingContainer_.size()];
    }
    const T& back() const
    {
        return underlyingContainer_[(front_+size_-1)%underlyingContainer_.size()];
    }
    [[nodiscard]] auto size() const
    {
        return size_;
    }
    [[nodiscard]] auto capacity() const
    {
        return underlyingContainer_.capacity();
    }

    [[nodiscard]] auto data()
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
}

#endif //NEKOLIB_QUEUE_H
