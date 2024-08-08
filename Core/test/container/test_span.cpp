//
// Created by unite on 08.08.2024.
//

#include "container/span.h"
#include <gtest/gtest.h>



TEST(Span, ConstructFromArray)
{
	std::array<int, 3> numbers{1,2,3};
	neko::Span<int> span(numbers);
	EXPECT_EQ(span.size(), 3);
	EXPECT_EQ(span[0],numbers[0]);
	EXPECT_EQ(span[1],numbers[1]);
	EXPECT_EQ(span[2],numbers[2]);

	int index = 0;
	for(auto& n : span)
	{
		EXPECT_EQ(n, numbers[index]);
		index++;
	}
}

TEST(Span, ConstructFromVector)
{
	std::vector<int> numbers{1,2,3};
	neko::Span<int> span(numbers);
	EXPECT_EQ(span.size(), 3);
	EXPECT_EQ(span[0],numbers[0]);
	EXPECT_EQ(span[1],numbers[1]);
	EXPECT_EQ(span[2],numbers[2]);

	int index = 0;
	for(auto& n : span)
	{
		EXPECT_EQ(n, numbers[index]);
		index++;
	}
}