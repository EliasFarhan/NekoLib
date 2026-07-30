//
// Created by unite on 05.06.2024.
//

#include <container/vector.h>

#include <algorithm>
#include <gtest/gtest.h>

TEST(SmallVector, Constructor)
{
    neko::SmallVector<int, 10> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(SmallVector, IntializerList)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    EXPECT_EQ(v.capacity(), 10);
    EXPECT_EQ(v.size(), 3);
}

TEST(SmallVector, PushBack)
{
    neko::SmallVector<int, 10> v;
    EXPECT_EQ(v.capacity(), 10);
    EXPECT_EQ(v.size(), 0);
    for(int i = 0; i < 10; i++)
    {
        v.push_back(i);
        EXPECT_EQ(v.size(), i+1);
        EXPECT_EQ(v.front(), 0);
    }

    EXPECT_THROW(v.push_back(10), std::out_of_range);

}


TEST(SmallVector, RangeBasedFor)
{
    neko::SmallVector<int, 10> v;
    EXPECT_EQ(v.capacity(), 10);
    EXPECT_EQ(v.size(), 0);
    for(int i = 0; i < 5; i++)
    {
        v.push_back(i);
        EXPECT_EQ(v.size(), i+1);
        EXPECT_EQ(v.front(), 0);
    }

    int count = 0;
    for(auto& elem: v)
    {
        EXPECT_EQ(count, elem);
        count++;
    }
    EXPECT_EQ(count, 5);
}

namespace
{
// Counts live instances, so a slot destroyed twice drives the count negative.
struct LifetimeCounted
{
    static int liveCount;
    LifetimeCounted() { liveCount++; }
    LifetimeCounted(const LifetimeCounted&) { liveCount++; }
    LifetimeCounted& operator=(const LifetimeCounted&) = default;
    ~LifetimeCounted() { liveCount--; }
};
int LifetimeCounted::liveCount = 0;
}

TEST(SmallVector, Clear)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    v.clear();
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

// clear() used to call ~T() on the live slots while the backing std::array destroyed every slot
// again at end of scope: a double destruction, and a use-after-destroy for anything pushed after.
// Every slot stays a live object now -- see the storage invariant on SmallVector.
TEST(SmallVector, ClearDoesNotDestroySlots)
{
    LifetimeCounted::liveCount = 0;
    {
        neko::SmallVector<LifetimeCounted, 4> v;
        EXPECT_EQ(LifetimeCounted::liveCount, 4); // all Capacity slots are constructed up front
        v.push_back({});
        v.push_back({});
        v.clear();
        EXPECT_EQ(v.size(), 0);
        EXPECT_EQ(LifetimeCounted::liveCount, 4); // logically empty, still 4 live objects
    }
    EXPECT_EQ(LifetimeCounted::liveCount, 0); // destroyed exactly once each, by the array
}

// The same defect seen from the resource side: assigning into a destroyed std::string was undefined
// behaviour, so a cleared container could not be reused.
TEST(SmallVector, ReuseAfterClearReleasesAndRestores)
{
    neko::SmallVector<std::string, 4> v;
    v.push_back("a string long enough to defeat the small-string optimisation");
    v.clear();
    EXPECT_EQ(v.size(), 0);
    EXPECT_TRUE(v[0].empty()); // clear() released the held buffer

    v.push_back("second use");
    EXPECT_EQ(v.size(), 1);
    EXPECT_EQ(v[0], "second use");
}

TEST(SmallVector, PopBack)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    v.pop_back();
    EXPECT_EQ(v.size(), 2);
    EXPECT_EQ(v.back(), 2);
    v.pop_back();
    v.pop_back();
    EXPECT_TRUE(v.empty());
    v.pop_back(); // popping an empty container is a no-op, not an underflow
    EXPECT_EQ(v.size(), 0);
}

TEST(SmallVector, TryPushBack)
{
    neko::SmallVector<int, 3> v;
    EXPECT_TRUE(v.try_push_back(1));
    EXPECT_TRUE(v.try_push_back(2));
    EXPECT_TRUE(v.try_push_back(3));
    EXPECT_FALSE(v.try_push_back(4)); // reports overflow rather than throwing
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v.back(), 3);
}

TEST(SmallVector, EmplaceBack)
{
    struct Pair
    {
        int a = 0;
        int b = 0;
    };

    neko::SmallVector<Pair, 2> v;
    auto& first = v.emplace_back(1, 2);
    EXPECT_EQ(first.a, 1);
    EXPECT_EQ(first.b, 2);
    EXPECT_EQ(v.size(), 1);

    EXPECT_TRUE(v.try_emplace_back(3, 4));
    EXPECT_EQ(v.back().b, 4);
    EXPECT_FALSE(v.try_emplace_back(5, 6));
    EXPECT_THROW(v.emplace_back(7, 8), std::out_of_range);
}

TEST(SmallVector, ResizeOverCapacityThrows)
{
    neko::SmallVector<int, 4> v;
    v.resize(4);
    EXPECT_EQ(v.size(), 4);
    // Unchecked before the fix: this wrote straight off the end of the backing array.
    EXPECT_THROW(v.resize(5), std::out_of_range);
    EXPECT_EQ(v.size(), 4);
}

TEST(SmallVector, EmptyAccessor)
{
    neko::SmallVector<int, 4> v;
    EXPECT_TRUE(v.empty());
    EXPECT_TRUE(v.is_empty());
    v.push_back(1);
    EXPECT_FALSE(v.empty());
}

TEST(SmallVector, Insert)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    auto it = v.insert(v.cbegin()+1, 4);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(v.size(), 4);
}

TEST(SmallVector, Erase)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    auto it = v.erase(v.begin()+1);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(v.size(), 2);

    neko::SmallVector<int, 10> v2 = {1, 2, 3};
    auto it2 = v2.erase(v2.cbegin()+1);
    EXPECT_EQ(*it2, 3);
    EXPECT_EQ(v2.size(), 2);
}

// The erase-remove idiom, which is how a caller compacts a container in place.
TEST(SmallVector, EraseRange)
{
    neko::SmallVector<int, 10> v = {1, 2, 3, 4, 5};
    v.erase(std::remove_if(v.begin(), v.end(), [](int i) { return i % 2 == 0; }), v.end());
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 3);
    EXPECT_EQ(v[2], 5);

    v.erase(v.begin(), v.end());
    EXPECT_TRUE(v.empty());
}

TEST(SmallVector, EraseRangeDoesNotDestroySlots)
{
    LifetimeCounted::liveCount = 0;
    {
        neko::SmallVector<LifetimeCounted, 4> v;
        v.resize(4);
        v.erase(v.begin() + 1, v.end());
        EXPECT_EQ(v.size(), 1);
        EXPECT_EQ(LifetimeCounted::liveCount, 4);
    }
    EXPECT_EQ(LifetimeCounted::liveCount, 0);
}

TEST(FixedVector, Constructor)
{
    neko::FixedVector<int, 10> v;
    EXPECT_EQ(v.capacity(), 10);
    EXPECT_EQ(v.size(), 0);
}

TEST(FixedVector, IntializerList)
{
    neko::FixedVector<int, 10> v = {1, 2, 3};
    EXPECT_EQ(v.capacity(), 10);
    EXPECT_EQ(v.size(), 3);
}

TEST(FixedVector, PushBack)
{
    neko::FixedVector<int, 10> v;

    constexpr int newValue = 5;

    v.push_back(newValue);
    EXPECT_EQ(v.size(), 1);
    EXPECT_EQ(v.front(), newValue);
}

TEST(FixedVector, PushOverCapacityDeathTest)
{
    neko::FixedVector<int, 10> v;

    for(std::size_t i = 0; i < v.capacity(); i++)
    {
        v.push_back(i);
    }
    EXPECT_THROW(v.push_back(0), std::out_of_range);
}

TEST(BasicVector, Construct)
{
    neko::BasicVector<int, 10> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

TEST(BasicVector, ConstructWithInitializerList)
{
    neko::BasicVector<int, 10> v{1,2,3};
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v.capacity(), 10);

    neko::BasicVector<int, 5> v2{1,2,3,4,5,6};
    EXPECT_EQ(v2.size(), 6);
    EXPECT_GT(v2.capacity(), 5);
}

TEST(BasicVector, RangeBasedFor)
{

    neko::BasicVector<int, 5> v2{1,2,3,4,5,6};

    int count = 0;
    for(auto& elem: v2)
    {
        EXPECT_EQ(v2[count], elem);
        count++;
    }
    EXPECT_EQ(count, v2.size());
}

TEST(BasicVector, Resize)
{
    neko::BasicVector<int, 5> v2;
    v2.resize(5);
    EXPECT_EQ(v2.size(), 5);

    v2.resize(3);
    EXPECT_EQ(v2.size(), 3);

    v2.resize(10);
    EXPECT_EQ(v2.size(), 10);
}

TEST(BasicVector, Algorithm)
{

    neko::BasicVector<int, 5> v2;
    v2.resize(5);

    int count = 0;
    for(auto& elem: v2)
    {
        EXPECT_EQ(v2[count], elem);
        count++;
    }
    EXPECT_EQ(count, v2.size());
}