#pragma once

#include "runtime/core/math/math_types.h"

#include <glm/gtc/quaternion.hpp>

namespace Blunder {

/// Linear interpolation for scalars
constexpr float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

/// Linear interpolation for vectors (uses GLM mix)
inline Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
  return glm::mix(a, b, t);
}

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
  return glm::mix(a, b, t);
}

inline Vec4 lerp(const Vec4& a, const Vec4& b, float t) {
  return glm::mix(a, b, t);
}

/// Spherical linear interpolation for quaternions
inline Quat slerp(const Quat& a, const Quat& b, float t) {
  return glm::slerp(a, b, t);
}

/// Normalized linear interpolation for quaternions (faster but less accurate)
inline Quat nlerp(const Quat& a, const Quat& b, float t) {
  return glm::normalize(a * (1.0f - t) + b * t);
}

/// Clamp value between min and max
template<typename T>
constexpr T clamp(const T& value, const T& minVal, const T& maxVal) {
  return value < minVal ? minVal : (value > maxVal ? maxVal : value);
}

/// Clamp float (explicit overload)
constexpr float clamp(float value, float minVal, float maxVal) {
  return value < minVal ? minVal : (value > maxVal ? maxVal : value);
}

/// Clamp vectors (uses GLM clamp)
inline Vec3 clamp(const Vec3& value, const Vec3& minVal, const Vec3& maxVal) {
  return glm::clamp(value, minVal, maxVal);
}

/// Saturate (clamp to 0-1 range)
constexpr float saturate(float value) {
  return clamp(value, 0.0f, 1.0f);
}

/// Smooth step interpolation (smooth acceleration and deceleration)
constexpr float smoothstep(float edge0, float edge1, float x) {
  float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

/// Smoother step (Ken Perlin's version)
constexpr float smootherstep(float edge0, float edge1, float x) {
  float t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/// Inverse lerp (find t given value between a and b)
constexpr float inverseLerp(float a, float b, float value) {
  return (value - a) / (b - a);
}

/// Remap value from one range to another
constexpr float remap(float value, float inMin, float inMax, float outMin, float outMax) {
  float t = inverseLerp(inMin, inMax, value);
  return lerp(outMin, outMax, t);
}

// Compile-time verification
static_assert(lerp(0.0f, 10.0f, 0.5f) == 5.0f, "lerp basic test");
static_assert(clamp(5.0f, 0.0f, 3.0f) == 3.0f, "clamp upper bound test");
static_assert(clamp(-1.0f, 0.0f, 1.0f) == 0.0f, "clamp lower bound test");

}  // namespace Blunder
