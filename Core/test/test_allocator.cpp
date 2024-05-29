#include "memory/allocator.h"
#include "memory/linear_allocator.h"
#include "memory/stack_allocator.h"
#include "memory/freelist_allocator.h"
#include "memory/pool_allocator.h"
#include "memory/proxy_allocator.h"

#include <gtest/gtest.h>

#include <random>


TEST(CustomAllocator, Alignment)
{
    int value = 3;
    int* ptr = &value;

    EXPECT_EQ(neko::CalculateAlignForwardAdjustment(ptr, alignof(int)), 0);
    EXPECT_EQ(neko::CalculateAlignForwardAdjustment((int*) ((std::uint64_t) ptr + 1), alignof(int)), 3);
    EXPECT_EQ(neko::CalculateAlignForwardAdjustment((int*) ((std::uint64_t) ptr + 2), alignof(int)), 2);
    EXPECT_EQ(neko::CalculateAlignForwardAdjustment((int*) ((std::uint64_t) ptr + 3), alignof(int)), 1);
    EXPECT_EQ(neko::CalculateAlignForwardAdjustment((int*)((std::uint64_t) ptr + 4), alignof(int)), 0);
    std::uint16_t header = 365;

    EXPECT_EQ(neko::CalculateAlignForwardAdjustmentWithHeader(ptr, alignof(int), sizeof(header)), 4);

}

TEST(Engine, TestLinearAllocator)
{
    const size_t length = 100;
    void* data = calloc(length + 1, sizeof(int));
    auto allocator = neko::LinearAllocator(data, sizeof(int) * (length + 1));

    for (size_t i = 0; i < length; i++)
    {
        int* v = (int*) allocator.Allocate(sizeof(int), alignof(int));
        if(v == nullptr)
        {
            std::cerr << "Linear allocator null at index: "<<i<<'\n';
            ASSERT_NE(v, nullptr);
        }
        *v = i;
    }
    allocator.Clear();
    std::free(data);

}

TEST(Engine, TestStackAllocator)
{
    const size_t length = 100;
    const size_t allocateNmb = 5;
    void* data = std::calloc(length * 2, sizeof(int));
    neko::StackAllocator allocator = neko::StackAllocator(sizeof(int) * (length * 2), data);
    std::vector<int*> ptr;
    ptr.reserve(length/allocateNmb);
    for (size_t i = 0; i < length/allocateNmb; i++)
    {
        int* v = (int*) allocator.Allocate(sizeof(int)*allocateNmb, alignof(int));
        ASSERT_NE(v, nullptr);
        ptr.push_back(v);
        for(size_t j = 0; j < allocateNmb; j++)
        {
            v[j] = j;
        }
    }
    std::for_each(ptr.rbegin(), ptr.rend(), [&allocator](int* p) { allocator.Deallocate(p); });
    free(data);

}

TEST(Engine, TestFreeListAllocator)
{
    const size_t length = 100;
    const size_t allocateNmb = 10;
    void* data = std::calloc(length * 2, sizeof(int));
    neko::FreeListAllocator allocator = neko::FreeListAllocator(sizeof(int) * (length * 2), data);
    std::vector<int*> ptr;
    ptr.reserve(length/allocateNmb);
    for (size_t i = 0; i < length/allocateNmb; i++)
    {
        int* v = (int*) allocator.Allocate(sizeof(int)*allocateNmb, alignof(int));
        ptr.push_back(v);
        for(size_t j = 0; j < allocateNmb; j++)
        {
            v[j] = j;
        }
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(ptr.begin(), ptr.end(), g);
    std::for_each(ptr.begin(), ptr.end(), [&allocator](int* p) { allocator.Deallocate(p); });
    std::free(data);

}

TEST(Engine, TestPoolAllocator)
{
    struct Prout
    {
        std::uint64_t id = 0;
        float radius = 0.0f;
    };
    const size_t length = 100;
    void* data = std::calloc(length+1, sizeof(Prout));
    neko::PoolAllocator<Prout> allocator(sizeof(Prout) * (length+1), data);
    std::vector<Prout*> ptr;
    ptr.reserve(length);
    for (size_t i = 0; i < length; i++)
    {
        Prout* v = (Prout*) allocator.Allocate(sizeof(Prout), alignof(Prout));
        ptr.push_back(v);
        v->id = i;
        v->radius = v->id / 3.0f;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(ptr.begin(), ptr.end(), g);
    std::for_each(ptr.begin(), ptr.end(), [&allocator](Prout* p) { allocator.Deallocate(p); });
    std::free(data);

}