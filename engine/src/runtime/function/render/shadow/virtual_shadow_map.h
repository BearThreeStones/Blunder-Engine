#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/math/coordinate_system.h"

namespace Blunder {

inline constexpr uint32_t k_vsm_page_texels = 128u;
inline constexpr uint32_t k_vsm_pages_per_axis = 128u;
inline constexpr uint32_t k_vsm_first_level = 6u;
inline constexpr uint32_t k_vsm_last_level = 10u;
inline constexpr uint32_t k_vsm_level_count =
    k_vsm_last_level - k_vsm_first_level + 1u;
inline constexpr uint32_t k_vsm_pages_per_level =
    k_vsm_pages_per_axis * k_vsm_pages_per_axis;
inline constexpr uint32_t k_vsm_virtual_pages =
    k_vsm_level_count * k_vsm_pages_per_level;
inline constexpr uint32_t k_vsm_physical_pages = 512u;
inline constexpr uint32_t k_vsm_unmarked_page = 0xFFFFFFFFu;
inline constexpr uint32_t k_max_point_shadow_maps = 8u;
inline constexpr uint32_t k_max_spot_shadow_maps = 8u;
inline constexpr uint32_t k_local_shadow_map_size = 512u;
inline constexpr float k_vsm_near_plane = 0.05f;
inline constexpr float k_vsm_far_plane = 2048.0f;
inline constexpr float k_point_shadow_near = 0.05f;

/// axis_x.w packing shared with pbr / deferred lighting shaders.
inline constexpr float k_shadow_code_none = 0.0f;
inline constexpr float k_shadow_code_directional = 1.0f;
inline constexpr float k_shadow_code_point_base = 2.0f;
inline constexpr float k_shadow_code_spot_base = 10.0f;

struct ShadowSamplingUniform {
  glm::vec4 vsm_origin_levels{0.0f, 0.0f, 0.0f, 6.0f};
  glm::vec4 vsm_params{10.0f, 128.0f, 1.0f / 128.0f, 0.0f};
  glm::mat4 vsm_light_view{1.0f};
  glm::mat4 spot_view_projections[8]{};
};

inline float vsmClipmapRadius(uint32_t level) {
  return static_cast<float>(1u << (level + 1u));
}

inline uint32_t vsmVirtualPageIndex(uint32_t level, uint32_t page_x,
                                    uint32_t page_y) {
  const uint32_t local = level - k_vsm_first_level;
  return local * k_vsm_pages_per_level + page_y * k_vsm_pages_per_axis + page_x;
}

inline uint64_t vsmPageTableBytes() {
  return sizeof(uint32_t) * static_cast<uint64_t>(k_vsm_virtual_pages);
}

inline uint64_t vsmPhysicalPoolBytes() {
  return static_cast<uint64_t>(k_vsm_physical_pages) * k_vsm_page_texels *
         k_vsm_page_texels * 4ull;
}

/// Camera-centered light view looking along `light_dir` (shine / emit, sun
/// onto the scene). Pass `-L`, not shading L: lookAt is −Z forward so
/// `glm::orthoZO` near/far contain casters. The old +Z basis plus looking
/// toward the sun left every page frustum empty.
inline glm::mat4 makeVsmLightView(const glm::vec3& light_dir,
                                  const glm::vec3& camera_pos) {
  glm::vec3 z = glm::length(light_dir) > 1e-4f ? glm::normalize(light_dir)
                                               : glm::vec3(0.45f, 0.7f, 0.55f);
  glm::vec3 up = kWorldUp;
  if (std::abs(glm::dot(z, up)) > 0.95f) {
    up = kWorldForward;
  }
  return glm::lookAt(camera_pos, camera_pos + z, up);
}

/// Stored VSM page depth for a lookAt / orthoZO light-space point.
inline float vsmLightSpaceClipDepth(float light_view_z) {
  const float z = (-light_view_z - k_vsm_near_plane) /
                  (k_vsm_far_plane - k_vsm_near_plane);
  return std::clamp(z, 0.0f, 1.0f);
}

inline glm::vec3 vsmWorldToLight(const glm::mat4& light_view,
                                 const glm::vec3& world) {
  const glm::vec4 p = light_view * glm::vec4(world, 1.0f);
  return glm::vec3(p);
}

/// Returns false when the light-space point is outside the clipmap level.
inline bool vsmLightToPage(const glm::vec3& light_pos, uint32_t level,
                           uint32_t& out_page_x, uint32_t& out_page_y,
                           glm::vec2& out_page_uv) {
  const float radius = vsmClipmapRadius(level);
  const float u = light_pos.x / (2.0f * radius) + 0.5f;
  const float v = light_pos.y / (2.0f * radius) + 0.5f;
  if (u < 0.0f || u >= 1.0f || v < 0.0f || v >= 1.0f) {
    return false;
  }
  const float scaled_x = u * static_cast<float>(k_vsm_pages_per_axis);
  const float scaled_y = v * static_cast<float>(k_vsm_pages_per_axis);
  out_page_x = static_cast<uint32_t>(scaled_x);
  out_page_y = static_cast<uint32_t>(scaled_y);
  if (out_page_x >= k_vsm_pages_per_axis) {
    out_page_x = k_vsm_pages_per_axis - 1u;
  }
  if (out_page_y >= k_vsm_pages_per_axis) {
    out_page_y = k_vsm_pages_per_axis - 1u;
  }
  out_page_uv = glm::vec2(scaled_x - static_cast<float>(out_page_x),
                          scaled_y - static_cast<float>(out_page_y));
  return true;
}

/// Virtual page covering `world` at `level`, if it is in range.
inline bool vsmWorldPageIndex(const glm::mat4& light_view,
                              const glm::vec3& world, uint32_t level,
                              uint32_t* out_index) {
  if (out_index == nullptr || level < k_vsm_first_level ||
      level > k_vsm_last_level) {
    return false;
  }
  uint32_t page_x = 0;
  uint32_t page_y = 0;
  glm::vec2 uv(0.0f);
  if (!vsmLightToPage(vsmWorldToLight(light_view, world), level, page_x, page_y,
                      uv)) {
    return false;
  }
  *out_index = vsmVirtualPageIndex(level, page_x, page_y);
  return true;
}

/// Marks the virtual page covering `world` at `level` if it is in range.
inline bool vsmMarkWorldPage(const glm::mat4& light_view,
                             const glm::vec3& world, uint32_t level,
                             uint32_t* page_flags) {
  uint32_t index = 0;
  if (page_flags == nullptr || !vsmWorldPageIndex(light_view, world, level, &index)) {
    return false;
  }
  page_flags[index] = 1u;
  return true;
}

struct VsmCompactResult {
  uint32_t assigned{0};
  uint32_t overflow{0};
};

/// Compact from a stamped virtual-page list. Unmarks the previous physical
/// slots instead of scanning all `k_vsm_virtual_pages` flags.
inline VsmCompactResult vsmCompactMarkedPages(const uint32_t* marked_virt,
                                              uint32_t marked_count,
                                              uint32_t* page_table,
                                              uint32_t* physical_to_virtual,
                                              uint32_t physical_capacity) {
  VsmCompactResult result{};
  if (page_table == nullptr) {
    return result;
  }
  if (physical_to_virtual != nullptr) {
    for (uint32_t i = 0; i < physical_capacity; ++i) {
      const uint32_t virt = physical_to_virtual[i];
      if (virt < k_vsm_virtual_pages) {
        page_table[virt] = k_vsm_unmarked_page;
      }
      physical_to_virtual[i] = k_vsm_unmarked_page;
    }
  }
  if (marked_virt == nullptr || marked_count == 0) {
    return result;
  }
  struct MarkedPage {
    uint32_t virt;
    uint32_t level;
    uint32_t dist2;
  };
  std::vector<MarkedPage> marked;
  marked.reserve(marked_count);
  const int32_t center = static_cast<int32_t>(k_vsm_pages_per_axis / 2u);
  for (uint32_t n = 0; n < marked_count; ++n) {
    const uint32_t i = marked_virt[n];
    if (i >= k_vsm_virtual_pages) {
      continue;
    }
    const uint32_t local = i % k_vsm_pages_per_level;
    const int32_t page_x = static_cast<int32_t>(local % k_vsm_pages_per_axis);
    const int32_t page_y = static_cast<int32_t>(local / k_vsm_pages_per_axis);
    const int32_t dx = page_x - center;
    const int32_t dy = page_y - center;
    marked.push_back(MarkedPage{i, i / k_vsm_pages_per_level,
                                static_cast<uint32_t>(dx * dx + dy * dy)});
  }
  std::sort(marked.begin(), marked.end(),
            [](const MarkedPage& a, const MarkedPage& b) {
              if (a.level != b.level) {
                return a.level < b.level;
              }
              if (a.dist2 != b.dist2) {
                return a.dist2 < b.dist2;
              }
              return a.virt < b.virt;
            });
  for (const MarkedPage& page : marked) {
    if (page_table[page.virt] != k_vsm_unmarked_page) {
      continue;
    }
    if (result.assigned >= physical_capacity) {
      ++result.overflow;
      continue;
    }
    page_table[page.virt] = result.assigned;
    if (physical_to_virtual != nullptr) {
      physical_to_virtual[result.assigned] = page.virt;
    }
    ++result.assigned;
  }
  return result;
}

/// CPU compact used by tests and as the host-side mirror of the GPU compact.
/// Overflow drops coarsest pages first, then pages farthest from clipmap center
/// (camera). Index order alone kept the −XY corner and missed the courtyard.
inline VsmCompactResult vsmCompactPages(const uint32_t* page_flags,
                                        uint32_t* page_table,
                                        uint32_t* physical_to_virtual,
                                        uint32_t physical_capacity) {
  VsmCompactResult result{};
  if (page_flags == nullptr || page_table == nullptr) {
    return result;
  }
  for (uint32_t i = 0; i < k_vsm_virtual_pages; ++i) {
    page_table[i] = k_vsm_unmarked_page;
  }
  if (physical_to_virtual != nullptr) {
    for (uint32_t i = 0; i < physical_capacity; ++i) {
      physical_to_virtual[i] = k_vsm_unmarked_page;
    }
  }
  struct MarkedPage {
    uint32_t virt;
    uint32_t level;
    uint32_t dist2;
  };
  std::vector<MarkedPage> marked;
  marked.reserve(512);
  const int32_t center = static_cast<int32_t>(k_vsm_pages_per_axis / 2u);
  for (uint32_t i = 0; i < k_vsm_virtual_pages; ++i) {
    if (page_flags[i] == 0u) {
      continue;
    }
    const uint32_t local = i % k_vsm_pages_per_level;
    const int32_t page_x = static_cast<int32_t>(local % k_vsm_pages_per_axis);
    const int32_t page_y = static_cast<int32_t>(local / k_vsm_pages_per_axis);
    const int32_t dx = page_x - center;
    const int32_t dy = page_y - center;
    marked.push_back(MarkedPage{i, i / k_vsm_pages_per_level,
                                static_cast<uint32_t>(dx * dx + dy * dy)});
  }
  std::sort(marked.begin(), marked.end(),
            [](const MarkedPage& a, const MarkedPage& b) {
              if (a.level != b.level) {
                return a.level < b.level;
              }
              if (a.dist2 != b.dist2) {
                return a.dist2 < b.dist2;
              }
              return a.virt < b.virt;
            });
  for (const MarkedPage& page : marked) {
    if (result.assigned >= physical_capacity) {
      ++result.overflow;
      continue;
    }
    page_table[page.virt] = result.assigned;
    if (physical_to_virtual != nullptr) {
      physical_to_virtual[result.assigned] = page.virt;
    }
    ++result.assigned;
  }
  return result;
}

inline glm::mat4 vsmPageViewProjection(const glm::mat4& light_view,
                                       uint32_t level, uint32_t page_x,
                                       uint32_t page_y) {
  const float radius = vsmClipmapRadius(level);
  const float page_size = (2.0f * radius) / static_cast<float>(k_vsm_pages_per_axis);
  const float left = -radius + static_cast<float>(page_x) * page_size;
  const float right = left + page_size;
  const float bottom = -radius + static_cast<float>(page_y) * page_size;
  const float top = bottom + page_size;
  glm::mat4 proj = glm::orthoZO(left, right, bottom, top, k_vsm_near_plane,
                                k_vsm_far_plane);
  proj[1][1] *= -1.0f;
  return proj * light_view;
}

}  // namespace Blunder
