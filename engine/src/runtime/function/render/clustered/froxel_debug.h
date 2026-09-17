#pragma once

#include "runtime/function/render/clustered/froxel_grid.h"

#include <algorithm>

#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace Blunder {

/// CPU twin of `occupancyHeatColorT` / `occupancyHeatColor` in
/// `engine/shaders/froxel_common.slang`.
inline float occupancyHeatColorT(uint32_t count) {
  return std::min(
      static_cast<float>(count) / static_cast<float>(k_froxel_lights_per_cell),
      1.0f);
}

inline glm::vec3 occupancyHeatColor(uint32_t count) {
  if (count == 0u) {
    return glm::vec3(0.04f, 0.04f, 0.06f);
  }
  const float t = occupancyHeatColorT(count);
  const glm::vec3 cold(0.05f, 0.15f, 0.55f);
  const glm::vec3 mid(0.15f, 0.75f, 0.25f);
  const glm::vec3 hot(0.95f, 0.15f, 0.05f);
  if (t < 0.5f) {
    return glm::mix(cold, mid, t * 2.0f);
  }
  return glm::mix(mid, hot, (t - 0.5f) * 2.0f);
}

}  // namespace Blunder
