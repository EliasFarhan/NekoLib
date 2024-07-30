//
// Created by unite on 05.06.2024.
//

#include <container/vector.h>
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

    EXPECT_DEATH(v.push_back(10), "");

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

TEST(SmallVector, Clear)
{
    neko::SmallVector<int, 10> v = {1, 2, 3};
    v.clear();
    EXPECT_EQ(v.size(), 0);
    EXPECT_EQ(v.capacity(), 10);

    class Destructor
    {
    public:
        ~Destructor()
        {
            checkValue = 0;
        }
        int checkValue = 1;
    };

    neko::SmallVector<Destructor, 10> v2;
    v2.push_back({});
    const auto& destructor = v2[0];
    v2.clear();
    EXPECT_EQ(destructor.checkValue, 0);
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

    for(int i = 0; i < v.capacity(); i++)
    {
        v.push_back(i);
    }
    EXPECT_DEATH(v.push_back(0), "");
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