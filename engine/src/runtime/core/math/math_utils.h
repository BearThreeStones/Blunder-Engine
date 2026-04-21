#pragma once

#include <cmath>

#include "EASTL/algorithm.h"
#include "EASTL/numeric_limits.h"
#include "runtime/core/math/math.h"
#include "runtime/core/math/math_types.h"

namespace Blunder {

// ============================================================================
// Math - Utility class with static math functions
// ============================================================================

class Math final {
 public:
  // -------------------------------------------------------------------------
  // Basic Math Operations
  // -------------------------------------------------------------------------

  static float abs(float value) { return std::fabs(value); }
  static bool isNan(float f) { return std::isnan(f); }
  static float sqr(float value) { return value * value; }
  static float sqrt(float value) { return std::sqrt(value); }
  static float invSqrt(float value) { return 1.0f / std::sqrt(value); }
  static float clamp(float v, float min, float max) {
    return eastl::clamp(v, min, max);
  }
  static float getMaxElement(float x, float y, float z) {
    return eastl::max({x, y, z});
  }

  static bool realEqual(
      float a, float b,
      float tolerance = eastl::numeric_limits<float>::epsilon());

  // -------------------------------------------------------------------------
  // Angle Conversion
  // -------------------------------------------------------------------------

  static float degreesToRadians(float degrees) { return degrees * DEG_TO_RAD; }
  static float radiansToDegrees(float radians) { return radians * RAD_TO_DEG; }

  static float angleUnitsToRadians(float units);
  static float radiansToAngleUnits(float radians);
  static float angleUnitsToDegrees(float units);
  static float degreesToAngleUnits(float degrees);

  // -------------------------------------------------------------------------
  // Trigonometric Functions
  // -------------------------------------------------------------------------

  static float sin(const Radian& rad) { return std::sin(rad.value()); }
  static float sin(float value) { return std::sin(value); }
  static float cos(const Radian& rad) { return std::cos(rad.value()); }
  static float cos(float value) { return std::cos(value); }
  static float tan(const Radian& rad) { return std::tan(rad.value()); }
  static float tan(float value) { return std::tan(value); }

  static Radian acos(float value);
  static Radian asin(float value);
  static Radian atan(float value) { return Radian(std::atan(value)); }
  static Radian atan2(float y_v, float x_v) {
    return Radian(std::atan2(y_v, x_v));
  }

  // -------------------------------------------------------------------------
  // Min/Max Templates
  // -------------------------------------------------------------------------

  template <typename T>
  static constexpr T max(const T a, const T b) {
    return (a > b) ? a : b;
  }

  template <typename T>
  static constexpr T min(const T a, const T b) {
    return (a < b) ? a : b;
  }

  template <typename T>
  static constexpr T max3(const T a, const T b, const T c) {
    return max(max(a, b), c);
  }

  template <typename T>
  static constexpr T min3(const T a, const T b, const T c) {
    return min(min(a, b), c);
  }

  // -------------------------------------------------------------------------
  // Matrix Construction
  // -------------------------------------------------------------------------

  /// Create a view matrix from position and orientation
  /// @param position Camera position
  /// @param orientation Camera orientation quaternion
  /// @param reflect_matrix Optional reflection matrix for planar reflections
  static Mat4 makeViewMatrix(const Vec3& position, const Quat& orientation,
                             const Mat4* reflect_matrix = nullptr);

  /// Create a look-at view matrix
  /// @param eye_position Camera position
  /// @param target_position Point to look at
  /// @param up_dir World up direction
  static Mat4 makeLookAtMatrix(const Vec3& eye_position,
                               const Vec3& target_position, const Vec3& up_dir);

  /// Create a perspective projection matrix
  /// @param fovy Vertical field of view
  /// @param aspect Aspect ratio (width/height)
  /// @param znear Near clipping plane
  /// @param zfar Far clipping plane
  static Mat4 makePerspectiveMatrix(Radian fovy, float aspect, float znear,
                                    float zfar);

  /// Create an orthographic projection matrix (depth range -1 to 1)
  static Mat4 makeOrthographicProjectionMatrix(float left, float right,
                                               float bottom, float top,
                                               float znear, float zfar);

  /// Create an orthographic projection matrix (depth range 0 to 1, for
  /// Vulkan/D3D)
  static Mat4 makeOrthographicProjectionMatrix01(float left, float right,
                                                 float bottom, float top,
                                                 float znear, float zfar);

 private:
  enum class AngleUnit { AU_DEGREE, AU_RADIAN };

  // Angle unit used by angleUnits* conversion functions
  static AngleUnit k_AngleUnit;

  // Private constructor - all methods are static
  Math() = default;
};

}  // namespace Blunder
