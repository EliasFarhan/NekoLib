//
// Created by unite on 30.07.2026.
//

#include <container/flat_map.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

TEST(SmallFlatMap, Constructor)
{
    neko::SmallFlatMap<int, int, 8> m;
    EXPECT_EQ(m.size(), 0);
    EXPECT_EQ(m.keys().capacity(), 8);
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

    const auto found = m.find(2);
    ASSERT_NE(found, m.end());
    EXPECT_EQ(found->second, 20);
    EXPECT_EQ(m.find(3), m.end());
    EXPECT_TRUE(m.contains(1));
    EXPECT_FALSE(m.contains(3));
}

// ⚠️ Pinned on purpose: std::flat_map restores its invariant after a throwing insert by CLEARING both
// containers, on MSVC and libc++ alike. Every caller that must survive a full map checks capacity first
// because of this; if a library ever stops clearing, this test is how we find out.
TEST(SmallFlatMap, OverflowThrowsAndClears)
{
    neko::SmallFlatMap<int, int, 2> m;
    m[1] = 1;
    m[2] = 2;
    EXPECT_EQ(m.size(), m.keys().capacity());
    EXPECT_THROW(m[3] = 3, std::bad_alloc);
    EXPECT_TRUE(m.empty());
}

TEST(SmallFlatMap, CheckCapacityBeforeInserting)
{
    neko::SmallFlatMap<int, int, 2> m;
    m.insert_or_assign(1, 10);
    m.insert_or_assign(2, 20);
    const bool full = m.size() == m.keys().capacity();
    EXPECT_TRUE(full);
    m.insert_or_assign(1, 11); // assigning an existing key needs no slot
    EXPECT_EQ(m[1], 11);
    EXPECT_EQ(m.size(), 2);
}

// Probing AND inserting into a string-keyed map with a std::string_view must not need a std::string.
TEST(SmallFlatMap, HeterogeneousStringKeys)
{
    neko::SmallFlatMap<std::string, int, 4> m;
    m[std::string_view{"spriteParams"}] = 1;
    m.try_emplace(std::string_view{"lightViewProj"}, 2);
    m.insert_or_assign(std::string_view{"lightViewProj"}, 3);

    const std::string_view key{"lightViewProj"};
    const auto found = m.find(key);
    ASSERT_NE(found, m.end());
    EXPECT_EQ(found->second, 3);
    EXPECT_TRUE(m.contains(std::string_view{"spriteParams"}));
    EXPECT_EQ(m.find(std::string_view{"absent"}), m.end());
    EXPECT_EQ(m.size(), 2);
}

TEST(SmallFlatMap, Erase)
{
    neko::SmallFlatMap<int, int, 4> m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;

    EXPECT_EQ(m.erase(2), 1);
    EXPECT_EQ(m.size(), 2);
    EXPECT_FALSE(m.contains(2));
    EXPECT_EQ(m[1], 10);
    EXPECT_EQ(m[3], 30);
    EXPECT_EQ(m.erase(2), 0);

    m.erase(m.find(3));
    EXPECT_EQ(m.erase(1), 1);
    EXPECT_TRUE(m.empty());
}

TEST(SmallFlatMap, IteratesInKeyOrder)
{
    neko::SmallFlatMap<int, int, 8> m;
    m[3] = 30;
    m[1] = 10;
    m[2] = 20;
    m.erase(2);
    m[4] = 40;

    std::vector<int> keys;
    for (const auto& [key, value] : m)
    {
        EXPECT_EQ(value, key * 10);
        keys.push_back(key);
    }
    EXPECT_EQ(keys, (std::vector<int>{1, 3, 4})); // the live entries only, sorted
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

TEST(SmallFlatMap, Copy)
{
    neko::SmallFlatMap<std::string, int, 4> m;
    m[std::string_view{"a"}] = 1;
    auto copy = m;
    copy[std::string_view{"b"}] = 2;
    EXPECT_EQ(m.size(), 1);
    EXPECT_EQ(copy.size(), 2);
}
