#ifndef NEKOLIB_STACK_ALLOCATOR_H
#define NEKOLIB_STACK_ALLOCATOR_H


#include "memory/allocator.h"
#include <exception>

namespace neko
{
class StackAllocator : public CustomAllocator
{
public:
    StackAllocator(std::size_t size, void* start) : CustomAllocator(start, size), currentPos_(start)
    {
        if(size == 0)
        {
            // Stack Allocator cannot be empty
            std::terminate();
        }
    }

    ~StackAllocator() override
    {
        currentPos_ = nullptr;
#if defined(NEKO_ASSERT)
        prevPos_ = nullptr;
#endif
    }

    StackAllocator(const StackAllocator&) = delete;

    StackAllocator& operator=(const StackAllocator&) = delete;

    void* Allocate(std::size_t allocatedSize, std::size_t alignment) override;

    void Deallocate(void* p) override;


    struct AllocationHeader
    {
#if defined(NEKO_ASSERT)
        void* prevPos = nullptr;
#endif
        std::uint8_t adjustment = 0;
    };

protected:
    void* currentPos_ = nullptr;
#if defined(NEKO_ASSERT)
    void* prevPos_ = nullptr;
#endif

};
}

#endif // NEKOLIB_STACK_ALLOCATOR_H