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
  /// x=style, y=alpha, z=line_width_scale or arc_start_rad (dial ghost), w=arc_delta_rad
  glm::vec4 params{0.0f};
  /// xy = center, zw = half extents in local XY plane
  glm::vec4 quad_layout{0.2f, 0.2f, 0.1f, 0.1f};
  float quad_z{0.05f};
  float _pad[3]{};
};

struct TransformGizmoDrawCounts {
  static constexpr uint32_t k_arrow_verts = 18u;

  static constexpr uint32_t k_plane_verts = 6u;

  static constexpr uint32_t k_center_sides = 12u;

  static constexpr uint32_t k_center_verts = k_center_sides * 6u;

  static constexpr uint32_t k_dial_sides = 48u;

  static constexpr uint32_t k_dial_verts = k_dial_sides * 6u;

  static constexpr uint32_t k_scale_box_verts = 6u * 6u;

};



}  // namespace Blunder


