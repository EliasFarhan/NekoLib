#include "memory/stack_allocator.h"

namespace neko
{


void* StackAllocator::Allocate(size_t allocatedSize, size_t alignment)
{
    if(allocatedSize == 0)
    {
        // Stack Allocator cannot allocate nothing
        return nullptr;
    }
    const auto adjustment = CalculateAlignForwardAdjustmentWithHeader(currentPos_, alignment, sizeof(AllocationHeader));
    if (usedMemory_ + adjustment + allocatedSize > size_)
    {
        // StackAllocator has not enough space for this allocation
        return nullptr;
    }

    void* alignedAddress = reinterpret_cast<void*>(reinterpret_cast<std::uint64_t>(currentPos_) + adjustment);

    auto* header = reinterpret_cast<AllocationHeader*>(reinterpret_cast<std::uint64_t>(alignedAddress) - sizeof(AllocationHeader));
    header->adjustment = static_cast<std::uint8_t>(adjustment);
#if defined(NEKO_ASSERT)
    header->prevPos = prevPos_;
    prevPos_ = alignedAddress;
#endif
    currentPos_ = reinterpret_cast<void*>(reinterpret_cast<std::uint64_t>(alignedAddress) + allocatedSize);
    usedMemory_ += allocatedSize + adjustment;
    numAllocations_++;
    return alignedAddress;

}


void StackAllocator::Deallocate(void* p)
{
    if(p == nullptr)
    {
        //Stack allocator requires valid pointer to deallocate
        return;
    }
#if defined(NEKO_ASSERT)
    if(p != prevPos_)
    {
        // Stack allocator needs to deallocate from the top
        std::terminate();
    } 
#endif
    //Access the AllocationHeader in the bytes before p
    auto* header = reinterpret_cast<AllocationHeader*>(reinterpret_cast<std::uint64_t>(p) - sizeof(AllocationHeader));
#if defined(NEKO_ASSERT)
    prevPos_ = header->prevPos;
#endif
    usedMemory_ -= reinterpret_cast<std::uint64_t>(currentPos_) - reinterpret_cast<std::uint64_t>(p) + header->adjustment;
    currentPos_ = reinterpret_cast<void*>(reinterpret_cast<std::uint64_t>(p) - header->adjustment);
    numAllocations_--;
}
}