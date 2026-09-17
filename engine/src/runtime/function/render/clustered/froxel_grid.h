#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/light_eval.h"

namespace Blunder {

/// Viewport clustered froxel grid defaults (Unreal Forward Light Grid topology).
inline constexpr uint32_t k_froxel_tile_size_px = 64;
inline constexpr uint32_t k_froxel_z_slices = 32;
inline constexpr uint32_t k_froxel_lights_per_cell = 64;
inline constexpr uint32_t k_max_clustered_lights = 256;
inline constexpr uint32_t k_clustered_mask_words =
    (k_max_clustered_lights + 31u) / 32u;

struct FroxelGridDim {
  uint32_t width_px{1};
  uint32_t height_px{1};
  uint32_t tiles_x{1};
  uint32_t tiles_y{1};
  uint32_t slices{k_froxel_z_slices};
  uint32_t tile_size_px{k_froxel_tile_size_px};
};

inline uint32_t ceilDivU32(uint32_t value, uint32_t denom) {
  return denom == 0 ? 0 : (value + denom - 1u) / denom;
}

inline FroxelGridDim makeFroxelGridDim(uint32_t width_px, uint32_t height_px) {
  FroxelGridDim dim{};
  dim.width_px = std::max(1u, width_px);
  dim.height_px = std::max(1u, height_px);
  dim.tile_size_px = k_froxel_tile_size_px;
  dim.slices = k_froxel_z_slices;
  dim.tiles_x = ceilDivU32(dim.width_px, dim.tile_size_px);
  dim.tiles_y = ceilDivU32(dim.height_px, dim.tile_size_px);
  return dim;
}

inline uint32_t froxelCount(const FroxelGridDim& dim) {
  return dim.tiles_x * dim.tiles_y * dim.slices;
}

inline uint32_t froxelIndexBufferCount(const FroxelGridDim& dim) {
  return froxelCount(dim) * k_froxel_lights_per_cell;
}

inline uint32_t froxelIndex(const FroxelGridDim& dim, uint32_t tile_x,
                            uint32_t tile_y, uint32_t slice) {
  return (slice * dim.tiles_y + tile_y) * dim.tiles_x + tile_x;
}

/// Exponential view-depth → slice in [0, slices). Near slices are thinner.
inline float viewZToSliceF(float view_z, float near_z, float far_z,
                           uint32_t slices) {
  const float n = std::max(near_z, 1e-4f);
  const float f = std::max(far_z, n + 1e-4f);
  const float z = std::max(view_z, n);
  const float t = std::log2(z / n) / std::log2(f / n);
  const float clamped = std::min(std::max(t, 0.0f), 0.999999f);
  return clamped * static_cast<float>(std::max(1u, slices));
}

inline uint32_t viewZToSlice(float view_z, float near_z, float far_z,
                             uint32_t slices) {
  const uint32_t n = std::max(1u, slices);
  const uint32_t idx = static_cast<uint32_t>(viewZToSliceF(view_z, near_z, far_z, n));
  return std::min(idx, n - 1u);
}

inline float sliceToViewZ(float slice, float near_z, float far_z,
                          uint32_t slices) {
  const float n = std::max(near_z, 1e-4f);
  const float f = std::max(far_z, n + 1e-4f);
  const float count = static_cast<float>(std::max(1u, slices));
  const float t = std::min(std::max(slice / count, 0.0f), 1.0f);
  return n * std::exp2(t * std::log2(f / n));
}

inline glm::vec3 unprojectView(const glm::mat4& inv_projection, glm::vec2 ndc,
                               float clip_z) {
  glm::vec4 view = inv_projection * glm::vec4(ndc.x, ndc.y, clip_z, 1.0f);
  if (std::abs(view.w) > 1e-8f) {
    view /= view.w;
  }
  return glm::vec3(view);
}

inline glm::vec3 viewPointAtDepth(const glm::mat4& inv_projection, glm::vec2 ndc,
                                  float view_z) {
  const glm::vec3 near_p = unprojectView(inv_projection, ndc, 0.0f);
  const glm::vec3 far_p = unprojectView(inv_projection, ndc, 1.0f);
  const float near_z = -near_p.z;
  const float far_z = -far_p.z;
  const float denom = far_z - near_z;
  const float t = std::abs(denom) > 1e-6f ? (view_z - near_z) / denom : 0.0f;
  return glm::mix(near_p, far_p, t);
}

inline void froxelViewAabb(const FroxelGridDim& dim,
                           const glm::mat4& inv_projection, uint32_t tile_x,
                           uint32_t tile_y, uint32_t slice, float near_z,
                           float far_z, glm::vec3& out_min, glm::vec3& out_max) {
  const float x0 = static_cast<float>(tile_x * dim.tile_size_px);
  const float y0 = static_cast<float>(tile_y * dim.tile_size_px);
  const float x1 = std::min(static_cast<float>((tile_x + 1u) * dim.tile_size_px),
                            static_cast<float>(dim.width_px));
  const float y1 = std::min(static_cast<float>((tile_y + 1u) * dim.tile_size_px),
                            static_cast<float>(dim.height_px));
  const float w = static_cast<float>(dim.width_px);
  const float h = static_cast<float>(dim.height_px);
  const glm::vec2 ndc[4] = {
      {x0 / w * 2.0f - 1.0f, y0 / h * 2.0f - 1.0f},
      {x1 / w * 2.0f - 1.0f, y0 / h * 2.0f - 1.0f},
      {x0 / w * 2.0f - 1.0f, y1 / h * 2.0f - 1.0f},
      {x1 / w * 2.0f - 1.0f, y1 / h * 2.0f - 1.0f},
  };
  const float z0 = sliceToViewZ(static_cast<float>(slice), near_z, far_z, dim.slices);
  const float z1 =
      sliceToViewZ(static_cast<float>(slice + 1u), near_z, far_z, dim.slices);
  out_min = glm::vec3(1e30f);
  out_max = glm::vec3(-1e30f);
  for (const glm::vec2& c : ndc) {
    const glm::vec3 p0 = viewPointAtDepth(inv_projection, c, z0);
    const glm::vec3 p1 = viewPointAtDepth(inv_projection, c, z1);
    out_min = glm::min(out_min, glm::min(p0, p1));
    out_max = glm::max(out_max, glm::max(p0, p1));
  }
}

inline bool sphereOverlapsAabb(const glm::vec3& center, float radius,
                               const glm::vec3& bmin, const glm::vec3& bmax) {
  const glm::vec3 closest = glm::clamp(center, bmin, bmax);
  const glm::vec3 d = center - closest;
  return glm::dot(d, d) <= radius * radius;
}

inline glm::vec3 lightViewPosition(const glm::mat4& view,
                                   const glm::vec3& world_position) {
  return glm::vec3(view * glm::vec4(world_position, 1.0f));
}

/// Viewport far is 100000 so centimetre Sponza can dolly out. 32 lighting
/// slices across that range park the courtyard in a handful of huge froxels
/// whose unprojected far corners are ill-conditioned. Cap to the mesh span.
inline float clusteredLightingFar(float camera_far, float scene_span_world,
                                  float camera_distance = 0.0f) {
  const float far_z = std::max(camera_far, 1.0f);
  if (scene_span_world <= kMeterSpaceTranslationMaxAbs * 2.0f) {
    return far_z;
  }
  // 2*span from the near plane is ~7400 for centimetre Sponza. A 3/4 LOOKAT
  // at 9000 sits the courtyard *behind* that cap, so GBuffer albedo never
  // lights and Unique gizmos sit on an empty grid.
  const float span_far = std::max(scene_span_world * 2.0f, 100.0f);
  const float zoom_far =
      std::max(camera_distance, 0.0f) + scene_span_world * 2.0f;
  return std::min(far_z, std::max(span_far, zoom_far));
}

struct FroxelFillResult {
  uint32_t dropped_assignments{0};
  uint32_t assigned_total{0};
};

/// CPU reference of the GPU froxel fill: stable light order, first 64 kept.
inline FroxelFillResult fillFroxelGridCpu(
    const FroxelGridDim& dim, const glm::mat4& view,
    const glm::mat4& inv_projection, float near_z, float far_z,
    const EvaluatedLight* lights, uint32_t light_count, uint32_t* indices,
    uint32_t* counts) {
  FroxelFillResult result{};
  const uint32_t n_froxels = froxelCount(dim);
  if (counts != nullptr) {
    std::fill(counts, counts + n_froxels, 0u);
  }
  const uint32_t cap = k_froxel_lights_per_cell;
  for (uint32_t slice = 0; slice < dim.slices; ++slice) {
    for (uint32_t ty = 0; ty < dim.tiles_y; ++ty) {
      for (uint32_t tx = 0; tx < dim.tiles_x; ++tx) {
        glm::vec3 bmin{};
        glm::vec3 bmax{};
        froxelViewAabb(dim, inv_projection, tx, ty, slice, near_z, far_z, bmin,
                       bmax);
        const uint32_t cell = froxelIndex(dim, tx, ty, slice);
        uint32_t written = 0;
        for (uint32_t li = 0; li < light_count; ++li) {
          const EvaluatedLight& light = lights[li];
          if (light.type != LightType::point && light.type != LightType::spot) {
            continue;
          }
          const glm::vec3 view_pos =
              lightViewPosition(view, light.world_position);
          if (!sphereOverlapsAabb(view_pos, light.range, bmin, bmax)) {
            continue;
          }
          if (written < cap) {
            if (indices != nullptr) {
              indices[cell * cap + written] = li;
            }
            ++written;
            ++result.assigned_total;
          } else {
            ++result.dropped_assignments;
          }
        }
        if (counts != nullptr) {
          counts[cell] = written;
        }
      }
    }
  }
  return result;
}

size_t buildClusteredPointSpotList(const SceneInstance& scene,
                                   EvaluatedLight* out_lights,
                                   size_t out_capacity);

}  // namespace Blunder
