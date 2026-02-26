#include <gtest/gtest.h>

TEST(DummyTest, AddZero)
{
    EXPECT_EQ(0 + 0, 0);
    EXPECT_EQ(0 + 3, 3);
    EXPECT_EQ(6 + 0, 6);
}

TEST(DummyTest, AddNegative)
{
    EXPECT_EQ(-2 + -4, -6);
    EXPECT_EQ(-7 + -2, -9);
}

TEST(DummyTest, AddPositive)
{
    EXPECT_EQ(2 + 4, 6);
    EXPECT_EQ(7 + 2, 9);
}
