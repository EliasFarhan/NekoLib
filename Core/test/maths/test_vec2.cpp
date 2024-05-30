#include "math/vec2.h"
#include "gtest/gtest.h"


struct Vec2fFixture : public ::testing::TestWithParam<std::pair<neko::Vec2f, neko::Vec2f>>
{};

TEST_P(Vec2fFixture, Add)
{
    auto [v1, v2] = GetParam();
    const auto result = v1+v2;
    EXPECT_FLOAT_EQ(result.x, v1.x+v2.x);
    EXPECT_FLOAT_EQ(result.y, v1.y+v2.y);
}

INSTANTIATE_TEST_SUITE_P(AllNumbers,
                         Vec2fFixture,
                         testing::Values(
                                 std::pair<neko::Vec2f, neko::Vec2f>{ {0.0f, 0.0f}, {1.2f,-3.4f} }
                         )
);