#pragma once

#include <cstdint>

namespace Blunder {

struct Fixed {
  static constexpr int kFracBits = 32;
  static constexpr int64_t kOne = 1LL << kFracBits;

  constexpr Fixed() = default;
  constexpr explicit Fixed(int64_t raw_value) : m_raw(raw_value) {}

  [[nodiscard]] static constexpr Fixed from_raw(int64_t raw_value) { return Fixed(raw_value); }
  [[nodiscard]] static constexpr Fixed from_int(int64_t integer) {
    return Fixed(integer << kFracBits);
  }

  [[nodiscard]] constexpr int64_t raw() const { return m_raw; }

  constexpr Fixed operator+(Fixed rhs) const { return Fixed(m_raw + rhs.m_raw); }
  constexpr Fixed operator-(Fixed rhs) const { return Fixed(m_raw - rhs.m_raw); }
  Fixed operator*(Fixed rhs) const;
  Fixed operator/(Fixed rhs) const;

  constexpr Fixed operator-() const { return Fixed(-m_raw); }

  constexpr bool operator==(Fixed rhs) const { return m_raw == rhs.m_raw; }
  constexpr bool operator!=(Fixed rhs) const { return m_raw != rhs.m_raw; }

  [[nodiscard]] static constexpr Fixed zero() { return Fixed(0); }

 private:
  int64_t m_raw = 0;
};

[[nodiscard]] Fixed sqrt(Fixed value);
[[nodiscard]] Fixed inv_sqrt(Fixed value);

struct FixedVec3 {
  Fixed x = Fixed::zero();
  Fixed y = Fixed::zero();
  Fixed z = Fixed::zero();

  constexpr FixedVec3() = default;
  constexpr FixedVec3(Fixed x_value, Fixed y_value, Fixed z_value)
      : x(x_value), y(y_value), z(z_value) {}

  constexpr FixedVec3 operator+(FixedVec3 rhs) const {
    return FixedVec3(x + rhs.x, y + rhs.y, z + rhs.z);
  }

  constexpr FixedVec3 operator-(FixedVec3 rhs) const {
    return FixedVec3(x - rhs.x, y - rhs.y, z - rhs.z);
  }

  FixedVec3 operator*(Fixed scalar) const {
    return FixedVec3(x * scalar, y * scalar, z * scalar);
  }

  FixedVec3 operator/(Fixed scalar) const {
    return FixedVec3(x / scalar, y / scalar, z / scalar);
  }
};

[[nodiscard]] Fixed dot(FixedVec3 lhs, FixedVec3 rhs);
[[nodiscard]] FixedVec3 normalize(FixedVec3 value);

struct FixedQuat {
  Fixed x = Fixed::zero();
  Fixed y = Fixed::zero();
  Fixed z = Fixed::zero();
  Fixed w = Fixed::from_int(1);

  constexpr FixedQuat() = default;
  constexpr FixedQuat(Fixed x_value, Fixed y_value, Fixed z_value, Fixed w_value)
      : x(x_value), y(y_value), z(z_value), w(w_value) {}
};

[[nodiscard]] Fixed dot(FixedQuat lhs, FixedQuat rhs);
[[nodiscard]] FixedQuat normalize(FixedQuat value);

}  // namespace Blunder
