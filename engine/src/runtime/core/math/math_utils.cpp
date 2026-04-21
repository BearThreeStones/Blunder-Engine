#include "runtime/core/math/math_utils.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {

// Static member initialization
Math::AngleUnit Math::k_AngleUnit = Math::AngleUnit::AU_RADIAN;

// ============================================================================
// Basic Math Operations
// ============================================================================

bool Math::realEqual(float a, float b, float tolerance) {
  return std::fabs(a - b) <= tolerance;
}

// ============================================================================
// Angle Unit Conversion
// ============================================================================

float Math::angleUnitsToRadians(float units) {
  if (k_AngleUnit == AngleUnit::AU_DEGREE) {
    return units * DEG_TO_RAD;
  }
  return units;
}

float Math::radiansToAngleUnits(float radians) {
  if (k_AngleUnit == AngleUnit::AU_DEGREE) {
    return radians * RAD_TO_DEG;
  }
  return radians;
}

float Math::angleUnitsToDegrees(float units) {
  if (k_AngleUnit == AngleUnit::AU_RADIAN) {
    return units * RAD_TO_DEG;
  }
  return units;
}

float Math::degreesToAngleUnits(float degrees) {
  if (k_AngleUnit == AngleUnit::AU_RADIAN) {
    return degrees * DEG_TO_RAD;
  }
  return degrees;
}

// ============================================================================
// Trigonometric Functions
// ============================================================================

Radian Math::acos(float value) {
  // Clamp to valid range [-1, 1] to avoid NaN
  if (value < -1.0f) value = -1.0f;
  if (value > 1.0f) value = 1.0f;
  return Radian(std::acos(value));
}

Radian Math::asin(float value) {
  // Clamp to valid range [-1, 1] to avoid NaN
  if (value < -1.0f) value = -1.0f;
  if (value > 1.0f) value = 1.0f;
  return Radian(std::asin(value));
}

// ============================================================================
// Matrix Construction
// ============================================================================

Mat4 Math::makeViewMatrix(const Vec3& position, const Quat& orientation, const Mat4* reflect_matrix) {
  // Build rotation matrix from quaternion
  Mat4 rotation = glm::mat4_cast(glm::inverse(orientation));

  // Build translation matrix
  Mat4 translation = glm::translate(Mat4(1.0f), -position);

  // View = Rotation * Translation
  Mat4 view = rotation * translation;

  // Apply reflection if provided
  if (reflect_matrix != nullptr) {
    view = view * (*reflect_matrix);
  }

  return view;
}

Mat4 Math::makeLookAtMatrix(const Vec3& eye_position, const Vec3& target_position, const Vec3& up_dir) {
  return glm::lookAt(eye_position, target_position, up_dir);
}

Mat4 Math::makePerspectiveMatrix(Radian fovy, float aspect, float znear, float zfar) {
  return glm::perspective(fovy.value(), aspect, znear, zfar);
}

Mat4 Math::makeOrthographicProjectionMatrix(float left, float right, float bottom, float top, float znear, float zfar) {
  return glm::ortho(left, right, bottom, top, znear, zfar);
}

Mat4 Math::makeOrthographicProjectionMatrix01(float left, float right, float bottom, float top, float znear, float zfar) {
  // GLM's orthoZO produces depth range [0, 1] instead of [-1, 1]
  return glm::orthoZO(left, right, bottom, top, znear, zfar);
}

}  // namespace Blunder
