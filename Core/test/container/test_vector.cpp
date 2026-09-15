//
// Created by unite on 05.06.2024.
//

#include <container/vector.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace
{
// Counts live instances, so a slot destroyed twice drives the count negative and a leaked one leaves
// it positive.
struct LifetimeCounted
{
    static int liveCount;
    int value = 0;
    LifetimeCounted() { liveCount++; }
    LifetimeCounted(int v) : value(v) { liveCount++; }
    LifetimeCounted(const LifetimeCounted& other) : value(other.value) { liveCount++; }
    LifetimeCounted& operator=(const LifetimeCounted&) = default;
    ~LifetimeCounted() { liveCount--; }
};
int LifetimeCounted::liveCount = 0;

// Not default-constructible: the container this replaced could not hold one at all.
struct NoDefault
{
    explicit NoDefault(int v) : value(v) {}
    int value;
};

// Throws from its copy constructor once armed, to exercise the partial-insert rollback.
struct ThrowOnCopy
{
    static int copiesBeforeThrow;
    static int liveCount;
    int value = 0;
    ThrowOnCopy(int v) : value(v) { liveCount++; }
    ThrowOnCopy(const ThrowOnCopy& other) : value(other.value)
    {
        if (copiesBeforeThrow-- == 0)
        {
            throw std::runtime_error("copy");
        }
        liveCount++;
    }
    ThrowOnCopy& operator=(const ThrowOnCopy&) = default;
    ~ThrowOnCopy() { liveCount--; }
};
int ThrowOnCopy::copiesBeforeThrow = 1000;
int ThrowOnCopy::liveCount = 0;

// A SmallVector of trivially copyable T is itself trivially copyable -- the property std::inplace_vector
// guarantees, and the one that lets such a container sit inside a memcpy'd struct.
static_assert(std::is_trivially_copyable_v<neko::SmallVector<int, 8>>);
static_assert(!std::is_trivially_copyable_v<neko::SmallVector<std::string, 8>>);
static_assert(std::is_nothrow_move_constructible_v<neko::SmallVector<std::unique_ptr<int>, 4>>);
static_assert(std::contiguous_iterator<neko::SmallVector<int, 4>::iterator>);
static_assert(std::ranges::contiguous_range<neko::SmallVector<int, 4>>);

// Constant initialisation of a global, which the app relies on to escape static-initialisation order.
constinit neko::SmallVector<std::string, 4> gConstantInitialised;
} // namespace

TEST(SmallVector, Constructor)
{
    neko::SmallVector<int, 10> v;
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
    EXPECT_EQ(v.max_size(), 10);
    EXPECT_TRUE(v.empty());
    EXPECT_TRUE(gConstantInitialised.empty());
}

TEST(SmallVector, IntializerList)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    EXPECT_EQ(v.capacity(), 10);
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v[2], 3);
    EXPECT_THROW((neko::SmallVector<int, 2>{1, 2, 3}), std::bad_alloc);
}

TEST(SmallVector, CountAndRangeConstructors)
{
    neko::SmallVector<int, 8> counted(3);
    EXPECT_EQ(counted.size(), 3);
    EXPECT_EQ(counted[1], 0); // value-initialised

    neko::SmallVector<int, 8> filled(4, 7);
    EXPECT_EQ(filled.size(), 4);
    EXPECT_EQ(filled.back(), 7);

    const std::vector<int> source = {5, 6, 7};
    neko::SmallVector<int, 8> fromIterators(source.begin(), source.end());
    EXPECT_EQ(fromIterators.size(), 3);
    neko::SmallVector<int, 8> fromRange(std::from_range, source);
    EXPECT_EQ(fromRange, fromIterators);
}

TEST(SmallVector, PushBack)
{
    neko::SmallVector<int, 10> v;
    for (int i = 0; i < 10; i++)
    {
        v.push_back(i);
        EXPECT_EQ(v.size(), i + 1);
        EXPECT_EQ(v.front(), 0);
    }

    EXPECT_THROW(v.push_back(10), std::bad_alloc);
    EXPECT_EQ(v.size(), 10);
}

TEST(SmallVector, OverflowErrorNamesTheBound)
{
    neko::SmallVector<int, 3> v = {1, 2, 3};
    try
    {
        v.push_back(4);
        FAIL();
    }
    catch (const neko::CapacityError& e)
    {
        EXPECT_NE(std::strstr(e.what(), "emplace_back"), nullptr);
        EXPECT_NE(std::strstr(e.what(), "3"), nullptr);
    }
}

TEST(SmallVector, RangeBasedFor)
{
    neko::SmallVector<int, 10> v;
    for (int i = 0; i < 5; i++)
    {
        v.push_back(i);
    }

    int count = 0;
    for (auto& elem : v)
    {
        EXPECT_EQ(count, elem);
        count++;
    }
    EXPECT_EQ(count, 5);

    int reversed = 4;
    for (auto it = v.rbegin(); it != v.rend(); ++it)
    {
        EXPECT_EQ(*it, reversed--);
    }
}

TEST(SmallVector, Clear)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    v.clear();
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);
}

// Only [0, size) is alive: nothing is constructed up front, and clear() destroys exactly what it held.
TEST(SmallVector, LifetimeFollowsSize)
{
    LifetimeCounted::liveCount = 0;
    {
        neko::SmallVector<LifetimeCounted, 4> v;
        EXPECT_EQ(LifetimeCounted::liveCount, 0);
        v.emplace_back(1);
        v.emplace_back(2);
        EXPECT_EQ(LifetimeCounted::liveCount, 2);
        v.clear();
        EXPECT_EQ(v.size(), 0);
        EXPECT_EQ(LifetimeCounted::liveCount, 0);
        v.emplace_back(3);
        EXPECT_EQ(LifetimeCounted::liveCount, 1);
    }
    EXPECT_EQ(LifetimeCounted::liveCount, 0);
}

TEST(SmallVector, ReuseAfterClear)
{
    neko::SmallVector<std::string, 4> v;
    v.push_back("a string long enough to defeat the small-string optimisation");
    v.clear();
    EXPECT_EQ(v.size(), 0);

    v.push_back("second use");
    EXPECT_EQ(v.size(), 1);
    EXPECT_EQ(v[0], "second use");
}

TEST(SmallVector, NonDefaultConstructible)
{
    neko::SmallVector<NoDefault, 4> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.insert(v.begin(), NoDefault{0});
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v[0].value, 0);
    EXPECT_EQ(v[2].value, 2);
}

TEST(SmallVector, MoveOnly)
{
    neko::SmallVector<std::unique_ptr<int>, 4> v;
    v.push_back(std::make_unique<int>(1));
    v.push_back(std::make_unique<int>(2));
    neko::SmallVector<std::unique_ptr<int>, 4> moved = std::move(v);
    EXPECT_EQ(moved.size(), 2);
    EXPECT_EQ(*moved[1], 2);

    neko::SmallVector<std::unique_ptr<int>, 4> other;
    other.push_back(std::make_unique<int>(9));
    std::swap(moved, other);
    EXPECT_EQ(moved.size(), 1);
    EXPECT_EQ(other.size(), 2);
    EXPECT_EQ(*other[0], 1);
}

TEST(SmallVector, CopyCopiesOnlyLiveElements)
{
    LifetimeCounted::liveCount = 0;
    {
        neko::SmallVector<LifetimeCounted, 8> v;
        v.emplace_back(1);
        v.emplace_back(2);
        neko::SmallVector<LifetimeCounted, 8> copy = v;
        EXPECT_EQ(LifetimeCounted::liveCount, 4);

        neko::SmallVector<LifetimeCounted, 8> shorter;
        shorter.emplace_back(7);
        copy = shorter; // shrinks by destroying the excess
        EXPECT_EQ(copy.size(), 1);
        EXPECT_EQ(copy[0].value, 7);
        EXPECT_EQ(LifetimeCounted::liveCount, 4);

        copy = v; // grows by constructing the surplus
        EXPECT_EQ(copy.size(), 2);
        EXPECT_EQ(copy[1].value, 2);
        EXPECT_EQ(LifetimeCounted::liveCount, 5);
    }
    EXPECT_EQ(LifetimeCounted::liveCount, 0);
}

// BattleCharacter assigns a path that may alias its own member.
TEST(SmallVector, SelfAssignment)
{
    neko::SmallVector<std::string, 4> v = {"a", "b"};
    const auto& alias = v;
    v = alias;
    EXPECT_EQ(v.size(), 2);
    EXPECT_EQ(v[1], "b");
}

TEST(SmallVector, SwapDifferentSizes)
{
    neko::SmallVector<std::string, 4> a = {"1", "2", "3"};
    neko::SmallVector<std::string, 4> b = {"x"};
    a.swap(b);
    EXPECT_EQ(a.size(), 1);
    EXPECT_EQ(a[0], "x");
    EXPECT_EQ(b.size(), 3);
    EXPECT_EQ(b[2], "3");
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
}

TEST(SmallVector, TryPushBack)
{
    neko::SmallVector<int, 3> v;
    EXPECT_NE(v.try_push_back(1), nullptr);
    EXPECT_NE(v.try_push_back(2), nullptr);
    const int* third = v.try_push_back(3);
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(*third, 3);
    EXPECT_EQ(v.try_push_back(4), nullptr); // reports overflow rather than throwing
    EXPECT_EQ(v.size(), 3);
    EXPECT_EQ(v.back(), 3);
}

TEST(SmallVector, EmplaceBackAggregate)
{
    struct Pair
    {
        int a = 0;
        int b = 0;
    };

    neko::SmallVector<Pair, 2> v;
    auto& first = v.emplace_back(1, 2); // parenthesised aggregate initialisation
    EXPECT_EQ(first.a, 1);
    EXPECT_EQ(first.b, 2);
    EXPECT_EQ(v.size(), 1);

    const Pair* second = v.try_emplace_back(3, 4);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->b, 4);
    EXPECT_EQ(v.try_emplace_back(5, 6), nullptr);
    EXPECT_THROW(v.emplace_back(7, 8), std::bad_alloc);
}

TEST(SmallVector, Resize)
{
    neko::SmallVector<std::string, 4> v;
    v.resize(4);
    EXPECT_EQ(v.size(), 4);
    EXPECT_THROW(v.resize(5), std::bad_alloc);
    EXPECT_EQ(v.size(), 4);
    v.resize(1);
    EXPECT_EQ(v.size(), 1);
    v.resize(3, "filled");
    EXPECT_EQ(v[2], "filled");
    EXPECT_THROW(v.reserve(5), std::bad_alloc);
}

TEST(SmallVector, At)
{
    neko::SmallVector<int, 4> v = {1};
    EXPECT_EQ(v.at(0), 1);
    EXPECT_THROW((void)v.at(1), std::out_of_range);
}

TEST(SmallVector, Insert)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    auto it = v.insert(v.cbegin() + 1, 4);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(v.size(), 4);
    EXPECT_EQ(v, (neko::SmallVector<int, 10>{1, 4, 2, 3}));

    it = v.insert(v.cend(), 2, 9);
    EXPECT_EQ(it, v.begin() + 4);
    EXPECT_EQ(v, (neko::SmallVector<int, 10>{1, 4, 2, 3, 9, 9}));

    const std::array<int, 2> more = {7, 8};
    v.insert(v.cbegin(), more.begin(), more.end());
    EXPECT_EQ(v, (neko::SmallVector<int, 10>{7, 8, 1, 4, 2, 3, 9, 9}));

    v.insert_range(v.cbegin() + 2, std::array<int, 1>{5});
    EXPECT_EQ(v.size(), 9);
    EXPECT_EQ(v[2], 5);
}

// An element of the container itself may be inserted: it is copied before anything moves.
TEST(SmallVector, InsertAliasingElement)
{
    neko::SmallVector<std::string, 4> v = {"first", "second"};
    v.insert(v.begin(), v[1]);
    EXPECT_EQ(v[0], "second");
    EXPECT_EQ(v[2], "second");
}

// A sized range that does not fit is refused before anything is constructed.
TEST(SmallVector, InsertRangeOverflowChangesNothing)
{
    neko::SmallVector<int, 4> v = {1, 2, 3};
    const std::array<int, 2> tooMany = {8, 9};
    EXPECT_THROW(v.insert(v.cbegin(), tooMany.begin(), tooMany.end()), std::bad_alloc);
    EXPECT_EQ(v, (neko::SmallVector<int, 4>{1, 2, 3}));
}

// A throw part-way through a range insert rolls the partial tail back.
TEST(SmallVector, InsertRangeThrowRollsBack)
{
    ThrowOnCopy::liveCount = 0;
    {
        neko::SmallVector<ThrowOnCopy, 8> v;
        v.emplace_back(1);
        v.emplace_back(2);
        const std::array<ThrowOnCopy, 3> source = {ThrowOnCopy{7}, ThrowOnCopy{8}, ThrowOnCopy{9}};
        ThrowOnCopy::copiesBeforeThrow = 1; // the second copy throws
        EXPECT_THROW(v.insert(v.cbegin(), source.begin(), source.end()), std::runtime_error);
        ThrowOnCopy::copiesBeforeThrow = 1000;
        EXPECT_EQ(v.size(), 2);
        EXPECT_EQ(v[0].value, 1);
        EXPECT_EQ(ThrowOnCopy::liveCount, 5); // 2 in v + 3 in source: nothing leaked
    }
    EXPECT_EQ(ThrowOnCopy::liveCount, 0);
}

TEST(SmallVector, AppendRange)
{
    neko::SmallVector<int, 4> v = {1};
    v.append_range(std::vector<int>{2, 3});
    EXPECT_EQ(v.size(), 3);

    const std::vector<int> source = {4, 5, 6};
    auto rest = v.try_append_range(source);
    EXPECT_EQ(v.size(), 4);
    EXPECT_EQ(v.back(), 4);
    EXPECT_EQ(*rest, 5);
}

TEST(SmallVector, Assign)
{
    neko::SmallVector<std::string, 4> v = {"a", "b", "c"};
    v.assign(2, "x");
    EXPECT_EQ(v, (neko::SmallVector<std::string, 4>{"x", "x"}));
    v = {"p", "q", "r"};
    EXPECT_EQ(v.size(), 3);
    v.assign_range(std::vector<std::string>{"z"});
    EXPECT_EQ(v.size(), 1);
    EXPECT_THROW(v.assign(5, "y"), std::bad_alloc);
}

TEST(SmallVector, Erase)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    auto it = v.erase(v.begin() + 1);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(v.size(), 2);

    neko::SmallVector<int, 10> v2 = {1, 2, 3};
    auto it2 = v2.erase(v2.cbegin() + 1);
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

TEST(SmallVector, EraseDestroysTheTail)
{
    LifetimeCounted::liveCount = 0;
    {
        neko::SmallVector<LifetimeCounted, 4> v;
        v.resize(4);
        EXPECT_EQ(LifetimeCounted::liveCount, 4);
        v.erase(v.begin() + 1, v.end());
        EXPECT_EQ(v.size(), 1);
        EXPECT_EQ(LifetimeCounted::liveCount, 1);
    }
    EXPECT_EQ(LifetimeCounted::liveCount, 0);
}

TEST(SmallVector, FreeEraseAndEraseIf)
{
    neko::SmallVector<int, 8> v = {1, 2, 1, 3, 1};
    EXPECT_EQ(erase(v, 1), 3);
    EXPECT_EQ(v, (neko::SmallVector<int, 8>{2, 3}));
    EXPECT_EQ(erase_if(v, [](int i) { return i > 2; }), 1);
    EXPECT_EQ(v.size(), 1);
}

TEST(SmallVector, Comparison)
{
    const neko::SmallVector<int, 4> a = {1, 2};
    const neko::SmallVector<int, 4> b = {1, 3};
    const neko::SmallVector<int, 4> c = {1, 2, 0};
    EXPECT_TRUE(a == a);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(b > c);
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

    for (std::size_t i = 0; i < v.capacity(); i++)
    {
        v.push_back(static_cast<int>(i));
    }
    EXPECT_THROW(v.push_back(0), std::out_of_range);
}
