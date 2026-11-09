#pragma once

#include <expected>
#include <string>

namespace math {
/**
   * @brief A demo Vector to show of the template
   * 
   */
class Vector {
 public:
  /**
  * @brief Destroy the darn vector
  * 
  */
  ~Vector() = default;

  /**
   * @brief Add two beautiful vectors
   * 
   * @param other the other vector to add to this
   * @return Vector a brand new vector with added results
   */
  [[nodiscard]] auto add(const Vector& other) const -> Vector;
  /**
   * @brief Sub two beautiful vectors
   * 
   * @param other the other vector to sub to this
   * @return Vector a brand new vector with subbed results
   */
  [[nodiscard]] auto sub(const Vector& other) const -> Vector;

  /**
   * @brief Div two ugly vectors
   * 
   * @param other the vector to div by
   * @return std::expected<Vector, std::string> a brand new vector or an error 
   */
  [[nodiscard]] auto div(const Vector& other) const
      -> std::expected<Vector, std::string>;

  /**
   * @brief The two beautiful integers representing a 2d vector
   * 
   */
  int x_, y_;
};
}  // namespace math
