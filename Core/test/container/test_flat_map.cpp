//
// Created by unite on 30.07.2026.
//

#include <container/flat_map.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

TEST(SmallFlatMap, Constructor)
{
    neko::SmallFlatMap<int, int, 8> m;
    EXPECT_EQ(m.size(), 0);
    EXPECT_EQ(m.capacity(), 8);
    EXPECT_TRUE(m.empty());
}

TEST(SmallFlatMap, SubscriptInsertsAndFinds)
{
    neko::SmallFlatMap<int, int, 4> m;
    m[1] = 10;
    m[2] = 20;
    EXPECT_EQ(m.size(), 2);
    EXPECT_EQ(m[1], 10);
    EXPECT_EQ(m[2], 20);
    EXPECT_EQ(m.size(), 2); // subscripting an existing key does not insert

    const int* found = m.find(2);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, 20);
    EXPECT_EQ(m.find(3), nullptr);
    EXPECT_TRUE(m.contains(1));
    EXPECT_FALSE(m.contains(3));
}

TEST(SmallFlatMap, SubscriptOverCapacityThrows)
{
    neko::SmallFlatMap<int, int, 2> m;
    m[1] = 1;
    m[2] = 2;
    EXPECT_TRUE(m.is_full());
    EXPECT_THROW(m[3] = 3, std::out_of_range);
    EXPECT_EQ(m[1], 1); // an existing key still resolves when full
}

TEST(SmallFlatMap, TryInsertOrAssign)
{
    neko::SmallFlatMap<int, int, 2> m;
    EXPECT_TRUE(m.try_insert_or_assign(1, 10));
    EXPECT_TRUE(m.try_insert_or_assign(2, 20));
    EXPECT_FALSE(m.try_insert_or_assign(3, 30)); // full: reported, not thrown
    EXPECT_TRUE(m.try_insert_or_assign(1, 11)); // assigning an existing key needs no slot
    EXPECT_EQ(m[1], 11);
    EXPECT_EQ(m.size(), 2);
}

// The reason this container exists rather than std::unordered_map: probing a string-keyed map with a
// std::string_view must not materialise a std::string.
TEST(SmallFlatMap, HeterogeneousStringLookup)
{
    neko::SmallFlatMap<std::string, int, 4> m;
    m[std::string{"spriteParams"}] = 1;
    m[std::string{"lightViewProj"}] = 2;

    const std::string_view key{"lightViewProj"};
    const int* found = m.find(key);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, 2);
    EXPECT_TRUE(m.contains(std::string_view{"spriteParams"}));
    EXPECT_EQ(m.find(std::string_view{"absent"}), nullptr);
}

TEST(SmallFlatMap, Erase)
{
    neko::SmallFlatMap<int, int, 4> m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;

    EXPECT_TRUE(m.erase(2));
    EXPECT_EQ(m.size(), 2);
    EXPECT_FALSE(m.contains(2));
    EXPECT_EQ(m[1], 10);
    EXPECT_EQ(m[3], 30); // survives the swap-into-the-hole
    EXPECT_FALSE(m.erase(2));

    EXPECT_TRUE(m.erase(3));
    EXPECT_TRUE(m.erase(1));
    EXPECT_TRUE(m.empty());
}

TEST(SmallFlatMap, Iteration)
{
    neko::SmallFlatMap<int, int, 8> m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;

    int visited = 0;
    int sum = 0;
    for(const auto& [key, value]: m)
    {
        visited++;
        sum += value;
        EXPECT_EQ(value, key * 10);
    }
    EXPECT_EQ(visited, 3); // iterates the live entries, not all Capacity slots
    EXPECT_EQ(sum, 60);
}

TEST(SmallFlatMap, ClearAndReuse)
{
    neko::SmallFlatMap<std::string, std::string, 4> m;
    m[std::string{"key"}] = "a value long enough to defeat the small-string optimisation";
    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_FALSE(m.contains(std::string_view{"key"}));

    m[std::string{"other"}] = "second use";
    EXPECT_EQ(m.size(), 1);
    EXPECT_EQ(m[std::string{"other"}], "second use");
}
