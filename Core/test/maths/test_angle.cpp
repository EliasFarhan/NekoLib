
#include "math/angle.h"
#include "gtest/gtest.h"

struct DegreeFixture : public ::testing::TestWithParam<std::pair<float, float>>
{};

TEST_P(DegreeFixture, Add)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);
    auto angle2 = neko::Degree(arg2);

    const auto result = angle1+angle2;
    EXPECT_FLOAT_EQ(result.value(), arg1+arg2);


    angle2 += angle1;
    EXPECT_FLOAT_EQ(angle2.value(), arg1+arg2);

}

TEST_P(DegreeFixture, Sub)
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

TEST_P(DegreeFixture, Mul)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);

    const auto result = angle1 * arg2;
    EXPECT_FLOAT_EQ(result.value(), arg1*arg2);

    const auto reverseResult = arg2 * angle1;
    EXPECT_FLOAT_EQ(reverseResult.value(), arg2*arg1);
}

TEST_P(DegreeFixture, Div)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Degree(arg1);

    const auto result = angle1 / arg2;
    EXPECT_FLOAT_EQ(result.value(), arg1 / arg2);
}

TEST_P(DegreeFixture, ConvertToRadian)
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
                         DegreeFixture,
                         testing::Values(
                                 std::pair{ 0.0f, 45.0f },
                                 std::pair {90.0f, 180.0f},
                                 std::pair{270.0f, -45.0f},
                                 std::pair{225.0f, 0.0f}
                         )
);

struct RadianFixture : public ::testing::TestWithParam<std::pair<float, float>>
{};


TEST_P(RadianFixture, Add)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Radian(arg1);
    auto angle2 = neko::Radian(arg2);

    const auto result = angle1+angle2;
    EXPECT_FLOAT_EQ(result.value(), arg1+arg2);


    angle2 += angle1;
    EXPECT_FLOAT_EQ(angle2.value(), arg1+arg2);

}

TEST_P(RadianFixture, Sub)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Radian(arg1);
    auto angle2 = neko::Radian(arg2);

    const auto result = angle1-angle2;
    EXPECT_FLOAT_EQ(result.value(), arg1-arg2);


    angle2 -= angle1;
    EXPECT_FLOAT_EQ(angle2.value(), arg2-arg1);

    const auto negResult = -angle1;
    EXPECT_FLOAT_EQ(negResult.value(), -arg1);

}


TEST_P(RadianFixture, Mul)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Radian(arg1);

    const auto result = angle1 * arg2;
    EXPECT_FLOAT_EQ(result.value(), arg1 * arg2);

    const auto reverseResult = arg2 * angle1;
    EXPECT_FLOAT_EQ(reverseResult.value(), arg2 * arg1);
}

TEST_P(RadianFixture, Div)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Radian(arg1);

    const auto result = angle1 / arg2;
    EXPECT_FLOAT_EQ(result.value(), arg1 / arg2);
}

TEST_P(RadianFixture, ConvertToDegree)
{
    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Radian(arg1);
    const auto angle2 = neko::Radian(arg2);

    const neko::Degree rad1 = angle1;
    const neko::Degree rad2 = angle2;

    EXPECT_FLOAT_EQ(rad1.value(), arg1 * 180.0f / neko::PI);
    EXPECT_FLOAT_EQ(rad2.value(), arg2 * 180.0f / neko::PI);
}

TEST_P(RadianFixture, Trigonometry)
{

    const auto [arg1, arg2] = GetParam();
    const auto angle1 = neko::Radian(arg1);
    const auto angle2 = neko::Radian(arg2);

    const auto cos1 = neko::Cos(angle1);
    const auto cos2 = neko::Cos(angle2);
    EXPECT_FLOAT_EQ(cos1, std::cos(arg1));
    EXPECT_FLOAT_EQ(cos2, std::cos(arg2));

    const auto sin1 = neko::Sin(angle1);
    const auto sin2 = neko::Sin(angle2);
    EXPECT_FLOAT_EQ(sin1, std::sin(arg1));
    EXPECT_FLOAT_EQ(sin2, std::sin(arg2));

    const auto tan1 = neko::Tan(angle1);
    const auto tan2 = neko::Tan(angle2);
    EXPECT_FLOAT_EQ(tan1, std::tan(arg1));
    EXPECT_FLOAT_EQ(tan2, std::tan(arg2));

    const auto acos1 = neko::Acos(cos1);
    const auto acos2 = neko::Acos(cos2);
    EXPECT_FLOAT_EQ(acos1.value(), std::acos(cos1));
    EXPECT_FLOAT_EQ(acos2.value(), std::acos(cos2));

    const auto asin1 = neko::Asin(sin1);
    const auto asin2 = neko::Asin(sin2);
    EXPECT_FLOAT_EQ(asin1.value(), std::asin(sin1));
    EXPECT_FLOAT_EQ(asin2.value(), std::asin(sin2));

    const auto atan1 = neko::Atan(tan1);
    const auto atan2 = neko::Atan(tan2);
    EXPECT_FLOAT_EQ(atan1.value(), std::atan(tan1));
    EXPECT_FLOAT_EQ(atan2.value(), std::atan(tan2));

    const auto secondAtan = neko::Atan2(arg1, arg2);
    EXPECT_FLOAT_EQ(secondAtan.value(), std::atan2(arg1, arg2));
}

INSTANTIATE_TEST_SUITE_P(AllNumbers,
                         RadianFixture,
                         testing::Values(
                                 std::pair{ 0.0f, neko::PI/8 },
                                 std::pair {neko::PI/4, neko::PI/2},
                                 std::pair{neko::PI*3/2, -neko::PI/8},
                                 std::pair{neko::PI*5/8, 0.0f}
                         )
);