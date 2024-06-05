//
// Created by unite on 05.06.2024.
//

#include <container/fixed_vector.h>
#include <gtest/gtest.h>

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