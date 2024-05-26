#include "memory/freelist_allocator.h"

namespace neko
{


FreeListAllocator::~FreeListAllocator()
{
    freeBlocks_ = nullptr;
}

void* FreeListAllocator::Allocate(std::size_t allocatedSize, std::size_t alignment)
{
    static_assert(sizeof(AllocationHeader) >= sizeof(FreeBlock), "sizeof(AllocationHeader) < sizeof(FreeBlock)");

    FreeBlock* prevFreeBlock = nullptr;
    FreeBlock* freeBlock = freeBlocks_;
    while (freeBlock != nullptr)
    {
        const std::uintptr_t adjustment = CalculateAlignForwardAdjustmentWithHeader(freeBlock, alignment, sizeof(AllocationHeader));
        std::size_t totalSize = allocatedSize + adjustment;
        if (freeBlock->size < totalSize)
        {
            prevFreeBlock = freeBlock;
            freeBlock = freeBlock->next;
            continue;
        }
        
        //If allocation takes the whole freeblock
        if (freeBlock->size - totalSize <= sizeof(AllocationHeader))
        {
            totalSize = freeBlock->size;
            if (prevFreeBlock != nullptr)
            {
                prevFreeBlock->next = freeBlock->next;
            }
            else
            {
                freeBlocks_ = freeBlock->next;
            }
        }
        else
        {
            FreeBlock* nextBlock = reinterpret_cast<FreeBlock*>(reinterpret_cast<std::uintptr_t>(freeBlock) + totalSize);
            nextBlock->size = freeBlock->size - totalSize;
            nextBlock->next = freeBlock->next;

            if (prevFreeBlock != nullptr)
            {
                prevFreeBlock->next = nextBlock;
            }
            else
            {
                freeBlocks_ = nextBlock;
            }
        }
        void* alignedAddress = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(freeBlock) + adjustment);
        auto* header = reinterpret_cast<AllocationHeader*>(reinterpret_cast<std::uint64_t>(alignedAddress) - sizeof(AllocationHeader));
        header->size = totalSize;
        header->adjustment = static_cast<std::uint8_t>(adjustment);
        usedMemory_ += totalSize;
        numAllocations_++;
        if(CalculateAlignForwardAdjustment(alignedAddress, alignment) != 0)
        {
            //Free List Allocator: New generated block is not aligned
            std::terminate();
        }
        return alignedAddress;
    }
    // FreeList Allocator has not enough space for this allocation
    return nullptr;
}

void FreeListAllocator::Deallocate(void* p)
{
    if(p == nullptr)
    {
        // Free List cannot deallocate nullptr
        return;
    }
    AllocationHeader* header = reinterpret_cast<AllocationHeader*>(reinterpret_cast<std::uintptr_t>(p) - sizeof(AllocationHeader));
    void* blockStart = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(p) - header->adjustment);
    size_t blockSize = header->size;
    void* blockEnd = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(blockStart) + blockSize);
    FreeBlock* prevFreeBlock = nullptr;
    FreeBlock* freeBlock = freeBlocks_;

    while (freeBlock != nullptr)
    {
        if (freeBlock >= blockEnd) break;
        prevFreeBlock = freeBlock;
        freeBlock = freeBlock->next;
    }
    if (prevFreeBlock == nullptr)
    {
        prevFreeBlock = static_cast<FreeBlock*>(blockStart);
        prevFreeBlock->size = blockSize;
        prevFreeBlock->next = freeBlocks_;
        freeBlocks_ = prevFreeBlock;
    }
    else if (reinterpret_cast<void*>(reinterpret_cast<size_t>(prevFreeBlock) + prevFreeBlock->size) == blockStart)
    {
        prevFreeBlock->size += blockSize;
    }
    else
    {
        auto* temp = static_cast<FreeBlock*>(blockStart);
        temp->size = blockSize;
        temp->next = prevFreeBlock->next;
        prevFreeBlock->next = temp;
        prevFreeBlock = temp;
    }
    if (freeBlock != nullptr && static_cast<void*>(freeBlock) == blockEnd)
    {
        prevFreeBlock->size += freeBlock->size;
        prevFreeBlock->next = freeBlock->next;
    }
    numAllocations_--;
    usedMemory_ -= blockSize;
}

}