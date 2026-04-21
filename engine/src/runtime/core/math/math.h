#pragma once

// Core type aliases
#include "runtime/core/math/math_types.h"

#include <cmath>

namespace Blunder {

// ============================================================================
// Math Constants
// ============================================================================

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = PI * 2.0f;
constexpr float HALF_PI = PI / 2.0f;
constexpr float INV_PI = 1.0f / PI;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

// ============================================================================
// Forward Declarations
// ============================================================================

class Degree;

// ============================================================================
// Radian - Type-safe angle in radians
// ============================================================================

class Radian final {
 public:
  constexpr Radian() = default;
  constexpr Radian(float value) : m_value(value) {}  // Allows Radian{0}
  constexpr explicit Radian(Degree deg);

  constexpr float value() const { return m_value; }

  // Arithmetic operators
  constexpr Radian operator+(Radian other) const { return Radian(m_value + other.m_value); }
  constexpr Radian operator-(Radian other) const { return Radian(m_value - other.m_value); }
  constexpr Radian operator*(float scalar) const { return Radian(m_value * scalar); }
  constexpr Radian operator/(float scalar) const { return Radian(m_value / scalar); }
  constexpr Radian operator-() const { return Radian(-m_value); }

  constexpr Radian& operator+=(Radian other) { m_value += other.m_value; return *this; }
  constexpr Radian& operator-=(Radian other) { m_value -= other.m_value; return *this; }
  constexpr Radian& operator*=(float scalar) { m_value *= scalar; return *this; }
  constexpr Radian& operator/=(float scalar) { m_value /= scalar; return *this; }

  // Comparison operators
  constexpr bool operator==(Radian other) const { return m_value == other.m_value; }
  constexpr bool operator!=(Radian other) const { return m_value != other.m_value; }
  constexpr bool operator<(Radian other) const { return m_value < other.m_value; }
  constexpr bool operator>(Radian other) const { return m_value > other.m_value; }
  constexpr bool operator<=(Radian other) const { return m_value <= other.m_value; }
  constexpr bool operator>=(Radian other) const { return m_value >= other.m_value; }

 private:
  float m_value{0.0f};
};

// Scalar * Radian
constexpr Radian operator*(float scalar, Radian rad) { return rad * scalar; }

// ============================================================================
// Degree - Type-safe angle in degrees
// ============================================================================

class Degree final {
 public:
  constexpr Degree() = default;
  constexpr Degree(float value) : m_value(value) {}  // Allows Degree{90}
  constexpr explicit Degree(Radian rad);

  constexpr float value() const { return m_value; }

  // Arithmetic operators
  constexpr Degree operator+(Degree other) const { return Degree(m_value + other.m_value); }
  constexpr Degree operator-(Degree other) const { return Degree(m_value - other.m_value); }
  constexpr Degree operator*(float scalar) const { return Degree(m_value * scalar); }
  constexpr Degree operator/(float scalar) const { return Degree(m_value / scalar); }
  constexpr Degree operator-() const { return Degree(-m_value); }

  constexpr Degree& operator+=(Degree other) { m_value += other.m_value; return *this; }
  constexpr Degree& operator-=(Degree other) { m_value -= other.m_value; return *this; }
  constexpr Degree& operator*=(float scalar) { m_value *= scalar; return *this; }
  constexpr Degree& operator/=(float scalar) { m_value /= scalar; return *this; }

  // Comparison operators
  constexpr bool operator==(Degree other) const { return m_value == other.m_value; }
  constexpr bool operator!=(Degree other) const { return m_value != other.m_value; }
  constexpr bool operator<(Degree other) const { return m_value < other.m_value; }
  constexpr bool operator>(Degree other) const { return m_value > other.m_value; }
  constexpr bool operator<=(Degree other) const { return m_value <= other.m_value; }
  constexpr bool operator>=(Degree other) const { return m_value >= other.m_value; }

 private:
  float m_value{0.0f};
};

// Scalar * Degree
constexpr Degree operator*(float scalar, Degree deg) { return deg * scalar; }

// ============================================================================
// Conversion Implementation (after both classes are defined)
// ============================================================================

constexpr Radian::Radian(Degree deg) : m_value(deg.value() * DEG_TO_RAD) {}
constexpr Degree::Degree(Radian rad) : m_value(rad.value() * RAD_TO_DEG) {}

// ============================================================================
// Conversion Functions
// ============================================================================

constexpr Radian toRadians(Degree deg) { return Radian(deg); }
constexpr Degree toDegrees(Radian rad) { return Degree(rad); }

constexpr Radian toRadians(float degrees) { return Radian(degrees * DEG_TO_RAD); }
constexpr Degree toDegrees(float radians) { return Degree(radians * RAD_TO_DEG); }

// ============================================================================
// Trigonometric Functions for Radian
// ============================================================================

inline float sin(Radian angle) { return std::sin(angle.value()); }
inline float cos(Radian angle) { return std::cos(angle.value()); }
inline float tan(Radian angle) { return std::tan(angle.value()); }

inline Radian asin(float value) { return Radian(std::asin(value)); }
inline Radian acos(float value) { return Radian(std::acos(value)); }
inline Radian atan(float value) { return Radian(std::atan(value)); }
inline Radian atan2(float y, float x) { return Radian(std::atan2(y, x)); }

// ============================================================================
// Compile-time Verification
// ============================================================================

static_assert(sizeof(Radian) == sizeof(float), "Radian size mismatch");
static_assert(sizeof(Degree) == sizeof(float), "Degree size mismatch");
static_assert(Radian{0}.value() == 0.0f, "Radian default value must be 0");
static_assert(Degree{90}.value() == 90.0f, "Degree construction test");

}  // namespace Blunder

// ============================================================================
// Additional Math Utilities
// ============================================================================

#include "runtime/core/math/transform.h"
#include "runtime/core/math/geometry.h"
#include "runtime/core/math/interpolation.h"
#include "runtime/core/math/math_utils.h"
