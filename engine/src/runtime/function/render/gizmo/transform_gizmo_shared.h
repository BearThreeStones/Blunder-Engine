#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Blunder {

struct TransformGizmoUniformData {
  glm::mat4 view{1.0f};
  glm::mat4 proj{1.0f};
  glm::mat4 gizmo_world{1.0f};
  glm::vec4 color{1.0f};
  /// x=style, y=alpha, z=arc_start_rad, w=arc_delta_rad (dial ghost)
  glm::vec4 params{0.0f};
};

struct TransformGizmoDrawCounts {
  static constexpr uint32_t k_arrow_verts = 6u * 6u + 12u * 6u;
  static constexpr uint32_t k_plane_verts = 6u;
  static constexpr uint32_t k_center_verts = 20u * 6u;
  static constexpr uint32_t k_dial_sides = 48u;
  static constexpr uint32_t k_dial_tube_sides = 6u;
  static constexpr uint32_t k_dial_verts =
      k_dial_sides * k_dial_tube_sides * 6u;
};

}  // namespace Blunder
