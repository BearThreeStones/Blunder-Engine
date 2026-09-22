#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Blunder {

void extractFrustumPlanes(const glm::mat4& view_projection, glm::vec4 out_planes[6]);

bool sphereInsideFrustum(const glm::vec3& center, float radius,
                         const glm::vec4 planes[6]);

/// MeshOptimizer sphere cone (RTR 4th 19.3): true when the Meshlet faces away.
bool meshletConeCulled(const glm::vec3& world_center, const glm::vec3& world_axis,
                       float cone_cutoff, float radius, const glm::vec3& camera);

bool meshletHiZOccluded(const glm::vec4& clip_center, float radius_ndc,
                        float sampled_min_depth);

/// Match meshlet_cull.slang: skip previous-frame Hi-Z once the camera is
/// tens of meshlet radii away (zoomed-out centimetre Sponza).
inline bool meshletSkipHiZWhenFar(float dist, float radius) {
  const float r = radius > 1e-4f ? radius : 1e-4f;
  return dist > 8.0f * r;
}

/// Sum per-batch compact counters (early or late cull count buffer).
inline uint32_t sumCompactCountBuffer(const uint32_t* counts, uint32_t count) {
  uint32_t total = 0;
  if (counts == nullptr) {
    return 0;
  }
  for (uint32_t i = 0; i < count; ++i) {
    total += counts[i];
  }
  return total;
}

}  // namespace Blunder
