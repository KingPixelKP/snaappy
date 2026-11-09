
#include "fmt/base.h"
#include "math/math.hpp"

#include <fmt/printf.h>

int main() {

  auto vec = math::Vector(1, 2);

  fmt::println("Vector: x:{}, y:{}", vec.x_, vec.y_);
}
