#ifndef NEKOLIB_UNIQUE_PTR_H
#define NEKOLIB_UNIQUE_PTR_H

#include <memory>

namespace neko
{

template<typename T, class Allocator = std::allocator<T>>
class UniquePtr final
{
public:
    constexpr UniquePtr()
    {
        allocator_ = Allocator();
    }

    constexpr explicit UniquePtr(T* ptr): ptr_(ptr)
    {
        allocator_ = Allocator();
    }

    constexpr UniquePtr(T* ptr, const Allocator& allocator): ptr_(ptr), allocator_(allocator)
    {
    }

    ~UniquePtr()
    {
        if(ptr_ != nullptr)
        {
            if constexpr (std::is_destructible_v<T>)
            {
                ptr_->~T();
            }
            allocator_.deallocate(ptr_, sizeof(T));
            ptr_ = nullptr;
        }
    }

    UniquePtr(const UniquePtr& other) = delete;
    constexpr UniquePtr(UniquePtr&& other) noexcept
    {
        std::swap(ptr_, other.ptr_);
        allocator_ = other.allocator_;
    }

    UniquePtr& operator=(const UniquePtr& other) = delete;
    constexpr UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        std::swap(ptr_, other.ptr_);
        allocator_ = other.allocator_;
        return *this;
    }

    constexpr T* get() const noexcept
    {
        return ptr_;
    }

    constexpr T* operator->() const noexcept
    {
        return ptr_;
    }
    constexpr T& operator*() const noexcept
    {
        return *ptr_;
    }

    constexpr bool operator==(const UniquePtr& other) const
    {
        return other.ptr_ == ptr_;
    }

    constexpr bool operator!=(const UniquePtr& other) const
    {
        return other.ptr_ != ptr_;
    }

    constexpr bool operator==(std::nullptr_t) const noexcept
    {
        return nullptr == ptr_;
    }

    constexpr bool operator!=(std::nullptr_t) const noexcept
    {
        return nullptr != ptr_;
    }

private:
    T* ptr_ = nullptr;
    Allocator allocator_;

};

template<typename T, class Allocator = std::allocator<T>>
bool operator==(std::nullptr_t other, UniquePtr<T, Allocator> ptr)
{
        return ptr == other;
}

template<typename T, class Allocator = std::allocator<T>>
bool operator!=(std::nullptr_t other, UniquePtr<T, Allocator> ptr)
{
    return ptr != other;
}

template<typename T, class... Types>
UniquePtr<T, std::allocator<T>> MakeUnique(Types... args)
{
    T* ptr = new T(args...);
    return UniquePtr<T, std::allocator<T>>(ptr);
}

template<typename T, class... Types, class Allocator = std::allocator<T>>
UniquePtr<T, Allocator> MakeUnique(Allocator allocator, Types... args)
{
    void* buff = allocator.allocate(sizeof(T));
    T* ptr = new (buff) T(args...);
    return UniquePtr<T, Allocator>(ptr, allocator);
}
}

#endif //UNIQUE_PTR_H
