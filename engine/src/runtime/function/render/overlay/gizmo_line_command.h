#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "EASTL/vector.h"

#include "runtime/function/render/gpu_driven/gpu_driven_cull.h"

namespace Blunder {

/// Packed overlay wire primitive. Must match `GizmoLineCommand` in
/// `engine/shaders/camera_gizmo.slang`.
struct GizmoLineCommand {
  glm::vec4 p0{0.0f};
  glm::vec4 p1{0.0f};
  glm::vec4 p2{0.0f};
  glm::vec4 color{1.0f};
  float style{0.0f};
  float pad0{0.0f};
  float pad1{0.0f};
  float pad2{0.0f};
};

static_assert(sizeof(GizmoLineCommand) == 80u,
              "GizmoLineCommand must match camera_gizmo.slang structured layout");

enum class GizmoLineDrawStyle : uint32_t {
  line = 0,
  triangle = 1,
  origin_disc = 2,
  icon_billboard = 3,
  light_icon_billboard = 4,
};

constexpr uint32_t k_gizmo_line_batch_verts = 6u;
constexpr uint64_t k_gizmo_line_fnv_offset = 14695981039346656037ull;
constexpr uint64_t k_gizmo_line_fnv_prime = 1099511628211ull;

inline uint64_t hashGizmoLineBytes(const void* data, size_t bytes,
                                   uint64_t hash = k_gizmo_line_fnv_offset) {
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < bytes; ++i) {
    hash ^= static_cast<uint64_t>(p[i]);
    hash *= k_gizmo_line_fnv_prime;
  }
  return hash;
}

inline uint64_t hashGizmoLineCommands(const GizmoLineCommand* commands,
                                      uint32_t count) {
  if (commands == nullptr || count == 0u) {
    return hashGizmoLineBytes(&count, sizeof(count));
  }
  uint64_t hash = hashGizmoLineBytes(&count, sizeof(count));
  return hashGizmoLineBytes(commands, count * sizeof(GizmoLineCommand), hash);
}

/// Screen-space unit normal of a clip-space line (pixel-width extrusion).
/// `clip_pos` is the homogeneous clip of the vertex, `clip_dir` is the clip
/// of that point plus the world line direction (w may be non-zero).
inline glm::vec2 gizmoClipLineScreenNormal(const glm::vec4& clip_pos,
                                           const glm::vec4& clip_dir,
                                           float viewport_width,
                                           float viewport_height) {
  const float w = clip_pos.w;
  if (std::fabs(w) < 1e-6f) {
    return glm::vec2(0.0f, 1.0f);
  }
  const glm::vec4 delta = (clip_dir - clip_pos * (clip_dir.w / w)) / w;
  glm::vec2 pixels(delta.x * viewport_width, delta.y * viewport_height);
  const float len = glm::length(pixels);
  if (len < 1e-8f) {
    return glm::vec2(0.0f, 1.0f);
  }
  pixels /= len;
  return glm::vec2(-pixels.y, pixels.x);
}

inline float gizmoWorldScaleMax(const glm::mat4& world) {
  const float sx = glm::length(glm::vec3(world[0]));
  const float sy = glm::length(glm::vec3(world[1]));
  const float sz = glm::length(glm::vec3(world[2]));
  return std::max(std::max(sx, sy), std::max(sz, 1e-6f));
}

inline bool gizmoSphereVisible(const glm::vec3& center, float radius,
                               const glm::vec4 frustum_planes[6]) {
  return sphereInsideFrustum(center, radius, frustum_planes);
}

inline uint32_t gizmoLineBatchedDrawCalls(uint32_t command_count) {
  return command_count == 0u ? 0u : 1u;
}

class GizmoLineCommandStream final {
 public:
  void clear() { m_commands.clear(); }

  void push(GizmoLineDrawStyle style, const glm::vec3& p0, const glm::vec3& p1,
            const glm::vec3& p2, const glm::vec4& color,
            float width_px = 0.0f) {
    GizmoLineCommand cmd{};
    cmd.p0 = glm::vec4(p0, 1.0f);
    cmd.p1 = glm::vec4(p1, 1.0f);
    cmd.p2 = glm::vec4(p2, 1.0f);
    cmd.color = color;
    cmd.style = static_cast<float>(static_cast<uint32_t>(style));
    cmd.pad0 = width_px;
    m_commands.push_back(cmd);
  }

  const eastl::vector<GizmoLineCommand>& commands() const { return m_commands; }
  uint32_t size() const { return static_cast<uint32_t>(m_commands.size()); }
  uint64_t hash() const {
    return hashGizmoLineCommands(m_commands.data(), size());
  }

 private:
  eastl::vector<GizmoLineCommand> m_commands;
};

}  // namespace Blunder
