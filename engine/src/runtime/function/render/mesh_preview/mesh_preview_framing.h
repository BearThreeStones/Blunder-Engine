#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "runtime/core/math/geometry.h"

namespace Blunder {

/// Fixed studio view direction for Mesh Preview stills (not scene light rig).
inline const glm::vec3 kMeshPreviewStudioViewDirection{0.55f, 0.35f, 0.75f};

struct MeshPreviewFramingParams {
  AABB local_bounds{};
  /// Multiplier applied to half-extents before fitting in view.
  float padding{1.15f};
  float aspect{1.0f};
  float vertical_fov_deg{40.0f};
};

struct MeshPreviewCameraFrame {
  glm::vec3 target{0.0f};
  glm::vec3 eye{0.0f};
  glm::vec3 up{0.0f, 0.0f, 1.0f};
  float vertical_fov_rad{0.0f};
  bool ok{false};
};

/// Frame `local_bounds` with padding; fixed elevated studio camera (Z-up).
inline MeshPreviewCameraFrame computeMeshPreviewCameraFrame(
    const MeshPreviewFramingParams& params) {
  MeshPreviewCameraFrame out{};
  if (!(params.aspect > 0.0f) || !(params.vertical_fov_deg > 0.0f)) {
    return out;
  }

  const glm::vec3 center = params.local_bounds.center();
  const glm::vec3 padded_half =
      params.local_bounds.extents() * std::max(params.padding, 1.0f);
  const float max_half = std::max(
      {padded_half.x, padded_half.y, padded_half.z, 1e-4f});

  const float v_fov = glm::radians(params.vertical_fov_deg);
  const float h_fov =
      2.0f * std::atan(std::tan(v_fov * 0.5f) * params.aspect);
  const float dist_v = max_half / std::tan(v_fov * 0.5f);
  const float dist_h = max_half / std::tan(h_fov * 0.5f);
  const float distance = std::max(dist_v, dist_h);

  const glm::vec3 view_dir =
      glm::normalize(kMeshPreviewStudioViewDirection);
  out.target = center;
  out.eye = center + view_dir * distance;
  out.up = glm::vec3(0.0f, 0.0f, 1.0f);
  out.vertical_fov_rad = v_fov;
  out.ok = true;
  return out;
}

}  // namespace Blunder
