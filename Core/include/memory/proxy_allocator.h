#ifndef NEKOLIB_PROXY_ALLOCATOR_H
#define NEKOLIB_PROXY_ALLOCATOR_H

#include "memory/allocator.h"

namespace neko
{

class ProxyAllocator final : public AllocatorInterface
{
public:
    ProxyAllocator(AllocatorInterface& allocator);

    void* Allocate(std::size_t size, std::size_t alignement) override;
    void Deallocate(void* ptr) override;
private:
    AllocatorInterface& allocator_;
};


}

#endif // NEKOLIB_PROXY_ALLOCATOR_H