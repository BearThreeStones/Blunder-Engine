#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"

namespace Blunder {

inline constexpr float kMeshPreviewOrbitYawSensitivity = 0.008f;
inline constexpr float kMeshPreviewOrbitPitchSensitivity = 0.008f;
inline constexpr float kMeshPreviewZoomStepFactor = 1.12f;
inline constexpr float kMeshPreviewMinDistanceMultiplier = 0.15f;
inline constexpr float kMeshPreviewMaxDistanceMultiplier = 8.0f;
inline constexpr float kMeshPreviewMinPitchRad = -1.48352986419f;  // -85 deg
inline constexpr float kMeshPreviewMaxPitchRad = 1.48352986419f;   // 85 deg

inline void meshPreviewEyeOffsetToOrbit(const glm::vec3& target, const glm::vec3& eye,
                                        float& yaw_rad, float& pitch_rad,
                                        float& distance) {
  const glm::vec3 offset = eye - target;
  distance = glm::length(offset);
  if (distance < 1e-4f) {
    yaw_rad = 0.0f;
    pitch_rad = 0.0f;
    return;
  }
  const glm::vec3 dir = offset / distance;
  pitch_rad = std::asin(std::clamp(dir.z, -1.0f, 1.0f));
  yaw_rad = std::atan2(dir.y, dir.x);
}

inline glm::vec3 meshPreviewOrbitToEye(const glm::vec3& target, float yaw_rad,
                                       float pitch_rad, float distance) {
  const float cos_pitch = std::cos(pitch_rad);
  const glm::vec3 dir(cos_pitch * std::cos(yaw_rad), cos_pitch * std::sin(yaw_rad),
                      std::sin(pitch_rad));
  return target + dir * std::max(distance, 0.1f);
}

struct MeshPreviewOrbitState {
  float yaw_offset_rad{0.0f};
  float pitch_offset_rad{0.0f};
  float distance_multiplier{1.0f};
};

/// Session-ephemeral orbit camera for Asset Inspector Mesh Preview.
class MeshPreviewOrbitCamera final {
 public:
  void setDefaultFrame(const MeshPreviewCameraFrame& frame) {
    m_default_frame = frame;
    reset();
  }

  void clear() {
    m_default_frame = {};
    reset();
  }

  const MeshPreviewCameraFrame& defaultFrame() const { return m_default_frame; }
  const MeshPreviewOrbitState& orbitState() const { return m_orbit; }
  bool hasUserOrbit() const { return m_has_user_orbit; }

  void orbit(float delta_x, float delta_y) {
    if (!m_default_frame.ok) {
      return;
    }
    if (std::fabs(delta_x) > 1e-6f || std::fabs(delta_y) > 1e-6f) {
      m_orbit.yaw_offset_rad += delta_x * kMeshPreviewOrbitYawSensitivity;
      m_orbit.pitch_offset_rad -= delta_y * kMeshPreviewOrbitPitchSensitivity;
      m_orbit.pitch_offset_rad =
          std::clamp(m_orbit.pitch_offset_rad, kMeshPreviewMinPitchRad,
                     kMeshPreviewMaxPitchRad);
      m_has_user_orbit = true;
    }
  }

  void zoom(float wheel_delta) {
    if (!m_default_frame.ok || std::fabs(wheel_delta) < 1e-6f) {
      return;
    }
    const float steps = wheel_delta;
    const float factor =
        std::pow(kMeshPreviewZoomStepFactor, -steps);
    m_orbit.distance_multiplier =
        std::clamp(m_orbit.distance_multiplier * factor,
                   kMeshPreviewMinDistanceMultiplier,
                   kMeshPreviewMaxDistanceMultiplier);
    m_has_user_orbit = true;
  }

  void reset() {
    m_orbit = {};
    m_has_user_orbit = false;
  }

  MeshPreviewCameraFrame currentFrame() const {
    if (!m_default_frame.ok) {
      return {};
    }
    if (!m_has_user_orbit) {
      return m_default_frame;
    }

    float yaw_rad = 0.0f;
    float pitch_rad = 0.0f;
    float distance = 0.0f;
    meshPreviewEyeOffsetToOrbit(m_default_frame.target, m_default_frame.eye, yaw_rad,
                                pitch_rad, distance);
    yaw_rad += m_orbit.yaw_offset_rad;
    pitch_rad = std::clamp(pitch_rad + m_orbit.pitch_offset_rad,
                           kMeshPreviewMinPitchRad, kMeshPreviewMaxPitchRad);
    distance *= m_orbit.distance_multiplier;

    MeshPreviewCameraFrame out = m_default_frame;
    out.eye = meshPreviewOrbitToEye(m_default_frame.target, yaw_rad, pitch_rad,
                                    distance);
    return out;
  }

 private:
  MeshPreviewCameraFrame m_default_frame{};
  MeshPreviewOrbitState m_orbit{};
  bool m_has_user_orbit{false};
};

}  // namespace Blunder
