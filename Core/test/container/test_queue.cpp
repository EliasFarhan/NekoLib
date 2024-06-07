//
// Created by unite on 05.06.2024.
//

#include <container/queue.h>
#include <gtest/gtest.h>


TEST(Queue, InitializerListConstructor)
{
    neko::Queue<int> q = {1,2,3};
    EXPECT_EQ(q.size(), 3);
    EXPECT_EQ(q.front(), 1);
    EXPECT_EQ(q.back(), 3);
}

TEST(Queue, PushAndPop)
{
    neko::Queue<int> q;

    for(int i = 0; i < 50; i++)
    {
        q.Push(i);
    }
    EXPECT_EQ(q.back(), 49);
    EXPECT_EQ(q.size(), 50);
    EXPECT_GE(q.capacity(), 50);
    const auto capacity = q.capacity();

    for(int i = 0; i < 25; i++)
    {
        EXPECT_EQ(q.front(), i);
        q.Pop();
    }

    EXPECT_EQ(q.back(), 49);
    EXPECT_EQ(q.size(), 25);
    EXPECT_EQ(q.capacity(), capacity);

    for(int i = 50; i < 75; i++)
    {
        q.Push(i);
        EXPECT_EQ(q.size(), i-25+1);
    }
    EXPECT_EQ(q.size(), 50);
    EXPECT_EQ(q.capacity(), capacity);
    EXPECT_EQ(q.front(), 25);
    EXPECT_EQ(q.back(), 74);

    q.Push(75);
    EXPECT_EQ(q.size(), 51);
    EXPECT_GE(q.capacity(), capacity);
    EXPECT_EQ(q.back(), 75);
}
