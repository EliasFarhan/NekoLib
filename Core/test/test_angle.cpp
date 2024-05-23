//
// Created by unite on 22.05.2024.
//

#include <math/angle.h>
#include <gtest/gtest.h>

struct DegreeOperationFixture :
public ::testing::TestWithParam<std::pair<float, float>>
{};

TEST_P(DegreeOperationFixture, Add)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);
    auto angle2 = neko::Degree(arg2);

    const auto result = angle1+angle2;
    EXPECT_FLOAT_EQ(result.value(), arg1+arg2);


    angle2 += angle1;
    EXPECT_FLOAT_EQ(angle2.value(), arg1+arg2);

}

TEST_P(DegreeOperationFixture, Sub)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);
    auto angle2 = neko::Degree(arg2);

    const auto result = angle1-angle2;
    EXPECT_FLOAT_EQ(result.value(), arg1-arg2);

    angle2 -= angle1;
    EXPECT_FLOAT_EQ(angle2.value(), arg2-arg1);

    const auto negAngle1 = -angle1;
    EXPECT_FLOAT_EQ(negAngle1.value(), -arg1);

}

TEST_P(DegreeOperationFixture, Mul)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);

    const auto result = angle1 * arg2;
    EXPECT_FLOAT_EQ(result.value(), arg1*arg2);

    const auto reverseResult = arg2 * angle1;
    EXPECT_FLOAT_EQ(reverseResult.value(), arg2*arg1);
}

TEST_P(DegreeOperationFixture, Div)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);

    const auto result = angle1 / arg2;
    EXPECT_FLOAT_EQ(result.value(), arg1 / arg2);
}

TEST_P(DegreeOperationFixture, ConvertToRadian)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);
    const auto angle2 = neko::Degree(arg2);

    const neko::Radian rad1 = angle1;
    const neko::Radian rad2 = angle2;

    EXPECT_FLOAT_EQ(rad1.value(), arg1 / 180.0f * neko::PI);
    EXPECT_FLOAT_EQ(rad2.value(), arg2 / 180.0f * neko::PI);
}

INSTANTIATE_TEST_SUITE_P(AllNumbers,
                         DegreeOperationFixture,
                         testing::Values(
                                 std::pair{ 0.0f, 45.0f },
                                 std::pair {90.0f, 180.0f},
                                 std::pair{270.0f, -45.0f},
                                 std::pair{225.0f, 0.0f}
                         )
);
