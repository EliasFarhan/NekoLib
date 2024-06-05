//
// Created by unite on 05.06.2024.
//

#include <container/queue.h>
#include <gtest/gtest.h>



TEST(Queue, PushAndPop)
{
    neko::Queue<int> q;

    for(int i = 0; i < 50; i++)
    {
        q.Push(i);
    }
    EXPECT_EQ(q.size(), 50);
    EXPECT_GE(q.capacity(), 50);
    const auto capacity = q.capacity();

    for(int i = 0; i < 25; i++)
    {
        q.Pop();
    }

    EXPECT_EQ(q.size(), 25);
    EXPECT_EQ(q.capacity(), capacity);

    for(int i = 50; i < 75; i++)
    {
        q.Push(i);
    }
    EXPECT_EQ(q.capacity(), capacity);
}