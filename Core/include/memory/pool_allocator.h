#ifndef NEKOLIB_POOL_ALLOCATOR_H
#define NEKOLIB_POOL_ALLOCATOR_H

#include "memory/allocator.h"

namespace neko
{

template<typename T>
class PoolAllocator : public CustomAllocator
{
    static_assert(sizeof(T) >= sizeof(void*));
public:
    PoolAllocator(std::size_t size, void* mem);

    ~PoolAllocator() override
    {
        freeBlocks_ = nullptr;
    }

    void* Allocate(std::size_t allocatedSize, std::size_t alignment) override;

    void Deallocate(void* p) override;

protected:
    struct FreeBlock
    {
        FreeBlock* next = nullptr;
    };
    FreeBlock* freeBlocks_ = nullptr;
};

template<typename T>
PoolAllocator<T>::PoolAllocator(std::size_t size, void* mem) : CustomAllocator(mem, size)
{
    const auto adjustment = CalculateAlignForwardAdjustment(mem, alignof(T));
    freeBlocks_ = reinterpret_cast<FreeBlock*>(reinterpret_cast<std::uintptr_t>(mem) + adjustment);
    FreeBlock* freeBlock = freeBlocks_;
    const size_t numObjects = (size - adjustment) / sizeof(T);
    for (size_t i = 0; i < numObjects - 1; i++)
    {
        freeBlock->next = reinterpret_cast<FreeBlock*>(reinterpret_cast<std::uintptr_t>(freeBlock) + sizeof(T));
        freeBlock = freeBlock->next;
    }
    freeBlock->next = nullptr;
}

template<typename T>
void* PoolAllocator<T>::Allocate(std::size_t allocatedSize, [[maybe_unused]] std::size_t alignment)
{
    if(!(allocatedSize == sizeof(T) && alignment == alignof(T)))
    {    
        //, "Pool Allocator can only allocate one Object pooled at once");
        std::terminate();
    }
    if (freeBlocks_ == nullptr)
    {
        // Pool Allocator is full
        return nullptr;
    }
    void* p = freeBlocks_;
    freeBlocks_ = freeBlocks_->next;
    usedMemory_ += allocatedSize;
    numAllocations_++;
    return p;
}

template<typename T>
void PoolAllocator<T>::Deallocate(void* p)
{
    auto* freeBlock = static_cast<FreeBlock*>(p);
    freeBlock->next = freeBlocks_;
    freeBlocks_ = freeBlock;
    usedMemory_ -= sizeof(T);
    numAllocations_--;
}
}
#endif // NEKOLIB_POOL_ALLOCATOR_H