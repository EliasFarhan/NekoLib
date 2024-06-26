#ifndef NEKOLIB_LINEAR_ALLOCATOR_H
#define NEKOLIB_LINEAR_ALLOCATOR_H

#include "memory/allocator.h"

namespace neko
{
    class LinearAllocator : public CustomAllocator
    {
    public:
        LinearAllocator(void* start, std::size_t size): CustomAllocator(start, size)
        {
            currentPos_ = start;
        }
        void* Allocate(std::size_t allocatedSize, std::size_t alignment) override;
        void Deallocate(void* ptr) override;
        void Clear();
        void Init(void* rootPtr, std::size_t size) override;
    private:
        mutable std::mutex mutex_;
        void* currentPos_ = nullptr;
    };
}

#endif //NEKOLIB_LINEAR_ALLOCATOR_H