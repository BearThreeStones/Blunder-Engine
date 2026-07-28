#pragma once

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {

constexpr float kMinCameraFovDegrees = 1.0f;
constexpr float kMaxCameraFovDegrees = 179.0f;
constexpr float kCameraClipMinDistance = 0.001f;
constexpr float kCameraClipEpsilon = 0.01f;

inline float clampCameraFovDegrees(float fov_degrees) {
  return std::clamp(fov_degrees, kMinCameraFovDegrees, kMaxCameraFovDegrees);
}

inline float halfHeightFromVerticalFovDegrees(float fov_degrees, float distance) {
  const float half_fov_rad = glm::radians(fov_degrees) * 0.5f;
  return distance * std::tan(half_fov_rad);
}

inline float verticalFovDegreesFromHalfHeight(float half_height, float distance) {
  if (distance <= 1e-6f) {
    return kMaxCameraFovDegrees;
  }
  const float half_fov_rad =
      std::atan(std::max(half_height, 0.0f) / distance);
  return clampCameraFovDegrees(glm::degrees(2.0f * half_fov_rad));
}

inline void clampCameraClipPlanes(float& near_clip, float& far_clip) {
  near_clip = std::max(near_clip, kCameraClipMinDistance);
  far_clip = std::max(far_clip, near_clip + kCameraClipEpsilon);
}

inline void setCameraNearClip(float& near_clip, float& far_clip, float new_near) {
  near_clip = std::max(new_near, kCameraClipMinDistance);
  if (far_clip < near_clip + kCameraClipEpsilon) {
    far_clip = near_clip + kCameraClipEpsilon;
  }
}

inline void setCameraFarClip(float& near_clip, float& far_clip, float new_far) {
  far_clip = std::max(new_far, near_clip + kCameraClipEpsilon);
}

}  // namespace Blunder
