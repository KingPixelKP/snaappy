#include "math/math.hpp"
#include <gtest/gtest.h>

TEST(VecTest, Create) {
  auto vec = math::Vector(1, 2);

  EXPECT_EQ(vec.x_, 1);
  EXPECT_EQ(vec.y_, 2);
}

TEST(VecTest, Add) {
  auto vec1 = math::Vector(1, 2);
  auto vec2 = math::Vector(3, -1);

  auto vec3 = vec1.add(vec2);

  EXPECT_EQ(vec3.x_, 4);
  EXPECT_EQ(vec3.y_, 1);
}

TEST(VecTest, Sub) {
  auto vec1 = math::Vector(1, 2);
  auto vec2 = math::Vector(3, -1);

  auto vec3 = vec1.sub(vec2);

  EXPECT_EQ(vec3.x_, -2);
  EXPECT_EQ(vec3.y_, 3);
}
