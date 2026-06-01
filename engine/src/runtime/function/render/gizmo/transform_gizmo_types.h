#pragma once

#include <cstdint>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Blunder {

enum class TransformGizmoMode : uint8_t {
  none = 0,
  translate,
  rotate,
  scale,
};

enum class GizmoSpace : uint8_t {
  global = 0,
  local,
};

enum class ManipulatorAxis : uint8_t {
  trans_x = 0,
  trans_y,
  trans_z,
  trans_xy,
  trans_yz,
  trans_zx,
  trans_c,
  rot_x,
  rot_y,
  rot_z,
  last,
};

enum class GizmoDrawStyle : uint32_t {
  arrow = 0,
  plane = 1,
  center = 2,
  dial = 3,
  dial_ghost = 4,
};

struct TransformGizmoMetrics {
  static constexpr float k_axis_length_factor = 0.12f;
  static constexpr float k_axis_radius_factor = 0.004f;
  static constexpr float k_cone_length_factor = 0.028f;
  static constexpr float k_cone_radius_factor = 0.012f;
  static constexpr float k_plane_half_extent_factor = 0.035f;
  static constexpr float k_center_radius_factor = 0.018f;
  static constexpr float k_dial_major_radius_factor = 0.85f;
  static constexpr float k_dial_tube_radius_factor = 0.06f;
  static constexpr float k_pick_slop = 1.25f;
  static constexpr float k_view_fade_dot_threshold = 0.92f;
  static constexpr float k_min_view_alpha = 0.25f;
};

glm::vec4 axisColorFor(ManipulatorAxis axis, bool highlight = false);
glm::vec4 centerColor(bool highlight = false);

glm::vec3 manipulatorLocalDirection(ManipulatorAxis axis);
void manipulatorTranslationMask(ManipulatorAxis axis, bool out_mask[3]);

bool isTranslationManipulator(ManipulatorAxis axis);
bool isRotationManipulator(ManipulatorAxis axis);

/// World-space rotation axis for dial handles.
glm::vec3 rotationAxisFor(ManipulatorAxis axis, const struct GizmoBasis& basis);

}  // namespace Blunder
