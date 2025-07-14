#ifndef NEKOLIB_FREELIST_ALLOCATOR_H
#define NEKOLIB_FREELIST_ALLOCATOR_H

#include "memory/allocator.h"
#include <exception>

namespace neko
{

class FreeListAllocator : public CustomAllocator
{
public:
    FreeListAllocator(std::size_t size, void* start) : CustomAllocator(start, size), freeBlocks_(static_cast<FreeBlock*>(start))
    {
        if(size <= sizeof(FreeBlock))
        {
            // Free List Allocator cannot be empty
            std::terminate();
        }
        freeBlocks_->size = size;
        freeBlocks_->next = nullptr;
    }

    ~FreeListAllocator() override;

    FreeListAllocator(const FreeListAllocator&) = delete;

    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

    void* Allocate(size_t allocatedSize, size_t alignment) override;

    void Deallocate(void* p) override;

protected:
    struct AllocationHeader
    {
        std::size_t size = 0;
        std::uint8_t adjustment = 0;
    };
    struct FreeBlock
    {
        std::size_t size = 0;
        FreeBlock* next = nullptr;
    };

    FreeBlock* freeBlocks_ = nullptr;
};

}
#endif // NEKOLIB_FREELIST_ALLOCATOR_H