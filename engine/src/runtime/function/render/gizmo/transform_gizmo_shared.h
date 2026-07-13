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
  float clip_enabled{0.0f};
  float line_width_px{0.0f};
  float viewport_height_px{1.0f};
  glm::vec4 clip_plane{0.0f, 0.0f, 1.0f, 0.0f};
  float dial_sides{48.0f};
  float _pad_dial_sides[3]{0.0f, 0.0f, 0.0f};
};

static_assert(sizeof(TransformGizmoUniformData) == 288u,
              "TransformGizmoUniformData must match transform_gizmo.slang std140 layout");

struct TransformGizmoDrawCounts {
  static constexpr uint32_t k_arrow_stem_verts = 6u;
  static constexpr uint32_t k_arrow_cone_sides = 8u;
  static constexpr uint32_t k_arrow_verts =
      k_arrow_stem_verts + k_arrow_cone_sides * 6u;

  static constexpr uint32_t k_plane_verts = 6u;

  /// Screen-space SDF ring quads (6 verts per ring).
  static constexpr uint32_t k_sdf_ring_verts = 6u;

  static constexpr uint32_t k_center_verts = k_sdf_ring_verts;

  static constexpr uint32_t k_dial_verts = k_sdf_ring_verts;

  static constexpr uint32_t k_dial_wire_verts = k_sdf_ring_verts;

  static constexpr uint32_t k_annulus_verts = k_sdf_ring_verts * 2u;

  static constexpr uint32_t k_dial_fill_verts = k_sdf_ring_verts;

  static constexpr uint32_t k_scale_box_verts = 6u * 6u;

  static constexpr uint32_t k_scale_stem_verts = 6u;

  static constexpr uint32_t k_scale_stem_box_verts =
      k_scale_stem_verts + k_scale_box_verts;

};



}  // namespace Blunder


