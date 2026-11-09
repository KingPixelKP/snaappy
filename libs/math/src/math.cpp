#include "math/math.hpp"
#include <expected>

namespace math {

auto Vector::add(const Vector& other) const -> Vector {
  return {.x_ = x_ + other.x_, .y_ = y_ + other.y_};
}
auto Vector::sub(const Vector& other) const -> Vector {
  return {.x_ = x_ - other.x_, .y_ = y_ - other.y_};
}
auto Vector::div(const Vector& other) const
    -> std::expected<Vector, std::string> {
  if (other.x_ == 0 || other.y_ == 0) {
    return std::unexpected("Bad Math!!!!");
  }
  return Vector{.x_ = x_ / other.x_, .y_ = y_ - other.y_};
}
}  // namespace math
