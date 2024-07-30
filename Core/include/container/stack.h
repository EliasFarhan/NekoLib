
#ifndef NEKOLIB_STACK_H
#define NEKOLIB_STACK_H

#include <vector>
#include <stack>

namespace neko
{
template<typename T>
using Stack = std::stack<T, std::vector<T>>;

template<typename T, std::size_t Capacity>
class FixedStack
{
public:
    T& Top()
    {
        if(top_ == 0)
        {
            std::terminate();
        }
        return underlyingContainer_[top_-1];
    }
    const T& Top() const
    {
        if(top_ == 0)
        {
            std::terminate();
        }
        return underlyingContainer_[top_-1];
    }

    void Push( const T& value )
    {
        if(top_ == Capacity)
        {
            std::terminate();
        }
        underlyingContainer_[top_] = value;
        top_++;
    }

    void Push( T&& value )
    {
        if(top_ == Capacity)
        {
            std::terminate();
        }
        underlyingContainer_[top_] = std::move(value);
        top_++;
    }

    void Pop()
    {
        if(top_ == 0)
        {
            std::terminate();
        }
        underlyingContainer_[top_] = {};
        top_--;
    }
private:
    std::array<T, Capacity> underlyingContainer_{};
    std::size_t top_ = 0;

};
}

#endif //NEKOLIB_STACK_H
