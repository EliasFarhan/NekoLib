//
// Created by unite on 04.07.2024.
//

#include "memory/unique_ptr.h"

#include "gtest/gtest.h"



TEST(UniquePtr, DefaultConstructor)
{
    neko::UniquePtr<int> number;

    EXPECT_EQ(number.get(), nullptr);
    EXPECT_EQ(number, nullptr);

}

TEST(UniquePtr, MakeUnique)
{
    auto number = neko::MakeUnique<int>(std::allocator<int>(), 3);

    EXPECT_NE(number.get(), nullptr);
    EXPECT_NE(number, nullptr);

}