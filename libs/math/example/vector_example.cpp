#include "math/math.hpp"

#include <fmt/base.h>

int main() {
  const auto lhs = math::Vector(2, 4);
  const auto rhs = math::Vector(-1, 3);
  const auto sum = lhs.add(rhs);

  fmt::println("sum = ({}, {})", sum.x_, sum.y_);
}
