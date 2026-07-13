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
  rot_t,
  rot_c,
  scale_c_outer,
  last,
};

enum class GizmoDrawStyle : uint32_t {
  arrow = 0,
  plane = 1,
  center = 2,
  dial = 3,
  dial_ghost = 4,
  scale_box = 5,
  dial_wire = 6,
  annulus = 7,
  dial_fill = 8,
  scale_stem_box = 9,
};

struct TransformGizmoMetrics {
  /// Blender U.gizmo_size default (RNA range 10–200).
  static constexpr float k_default_gizmo_size = 75.0f;
  static constexpr float k_min_gizmo_size = 10.0f;
  static constexpr float k_max_gizmo_size = 200.0f;
  static constexpr float k_ui_scale_factor = 1.0f;
  /// Match navigate gizmo max fraction of min(viewport_w, viewport_h).
  static constexpr float k_viewport_gizmo_size_factor = 0.35f;

  /// Blender transform_gizmo_3d WM_gizmo_set_scale scale_basis values.
  static constexpr float k_translate_handle_scale_basis = 1.0f;
  static constexpr float k_translate_center_scale_basis = 0.5f;
  static constexpr float k_rotate_axis_dial_scale_basis = 1.0f;
  static constexpr float k_rotate_outer_ring_scale_basis = 1.2f;
  static constexpr float k_scale_outer_ring_scale_basis = 0.2f;

  /// Deprecated alias — axis dials use k_rotate_axis_dial_scale_basis.
  static constexpr float k_rotate_dial_scale_basis = k_rotate_axis_dial_scale_basis;

  /// Local mesh extents from transform_gizmo.slang (must match shader).
  static constexpr float k_mesh_arrow_length = 1.0f;
  static constexpr float k_mesh_stem_radius = 0.025f;
  /// Blender ED_GIZMO_ARROW_STYLE_PLANE local scale (~0.1).
  static constexpr float k_mesh_plane_half_extent = 0.08f;
  /// Offset from pivot along each plane axis (Blender plane handles sit in axis corners).
  static constexpr float k_mesh_plane_center_offset = 0.5f;
  static constexpr float k_mesh_center_radius = 0.20f;
  /// Blender imm_draw_circle_wire_3d radius 1.0 at axis dials.
  static constexpr float k_mesh_dial_major_radius = 1.0f;
  static constexpr float k_mesh_outer_ring_radius = 1.0f;
  static constexpr float k_arc_inner_factor = 6.0f;

  /// Blender DIAL_CLIP_BIAS — clip plane offset to reduce edge flicker.
  static constexpr float k_dial_clip_bias = 0.02f;
  static constexpr float k_mesh_scale_stem_start = k_mesh_plane_center_offset;
  static constexpr float k_mesh_scale_box_center_offset = 0.8f;
  static constexpr float k_mesh_scale_box_half_extent = 0.10f;
  static constexpr float k_mesh_scale_center_half_extent = 0.12f;
  /// Blender MAN_AXIS_SCALE_PLANE_SCALE.
  static constexpr float k_scale_plane_length_factor = 0.7f;
  /// Blender MAN_AXIS_ROT_T trackball alpha factor.
  static constexpr float k_trackball_alpha_factor = 0.05f;

  static constexpr float k_pick_slop = 1.25f;

  /// Blender GIZMO_AXIS_LINE_WIDTH; WM_gizmo_set_line_width(rot axis) adds +1.0f.
  static constexpr float k_axis_line_width = 2.0f;
  static constexpr float k_rotate_line_width = 3.0f;
  /// Screen-space SDF stroke width (px) for arrow stems, plane borders, scale stems.
  static constexpr float k_sdf_stroke_width_px = 0.3f;
  /// Legacy Blender plane border width (superseded by k_sdf_stroke_width_px for SDF paths).
  static constexpr float k_plane_line_width = 1.0f;
  /// Screen-space polyline widths (pixels) for dial styles.
  static constexpr float k_dial_polyline_width_px = k_rotate_line_width;
  static constexpr float k_outer_ring_polyline_width_px = k_axis_line_width;

  /// Ring tessellation: segment count scales with on-screen radius (see computeDialSides).
  static constexpr uint32_t k_dial_sides_min = 48u;
  static constexpr uint32_t k_dial_sides_max = 1024u;
  static constexpr float k_dial_target_chord_px = 2.0f;

  /// Blender gizmo_get_axis_color alpha values (normal / highlight).
  static constexpr float k_gizmo_color_alpha = 0.6f;
  static constexpr float k_gizmo_color_alpha_hi = 1.0f;

  /// Blender g_tw_axis_range (regular axis vs plane handles).
  static constexpr float k_axis_idot_fade_min = 0.02f;
  static constexpr float k_axis_idot_fade_max = 0.1f;
  static constexpr float k_plane_idot_fade_min = 0.175f;
  static constexpr float k_plane_idot_fade_max = 0.25f;
};

/// Blender gizmo_color_get pair (color + color_hi).
struct GizmoAxisColor {
  glm::vec4 color{1.0f};
  glm::vec4 color_hi{1.0f};
};

/// Per-handle scale_basis (Blender WM_gizmo_set_scale).
float gizmoScaleBasisForAxis(ManipulatorAxis axis);

/// Blender gizmo_orientation_axis: world axis index for color/fade (0=X, 1=Y, 2=Z).
uint32_t gizmoOrientationAxis(ManipulatorAxis axis, bool* out_is_plane = nullptr);

bool isPlaneManipulator(ManipulatorAxis axis);

/// Blender gizmo_get_axis_color fade factor [0, 1]; 0 means hidden.
float gizmoAxisFadeFactor(ManipulatorAxis axis, const float idot[3]);

/// Blender gizmo_get_axis_color + gizmo_color_get selection.
GizmoAxisColor gizmoAxisColor(ManipulatorAxis axis, const float idot[3]);

/// True when hover should use color_hi (false for WM_GIZMO_DRAW_HOVER decor: rot_t, rot_c, scale annulus).
bool gizmoHandleUsesHoverColorHi(ManipulatorAxis axis);

/// Blender gizmo_color_get — pick color or color_hi (transform handles are always drawn).
glm::vec4 gizmoColorGet(ManipulatorAxis axis, const float idot[3], bool highlight);

/// Mesh thickness scale relative to k_axis_line_width (stem radius in shader).
float gizmoLineWidthScale(ManipulatorAxis axis, GizmoDrawStyle style);

/// Screen-space SDF stroke width in pixels (arrow stem, plane border, scale stem); 0 otherwise.
float gizmoSdfStrokeWidthPx(GizmoDrawStyle style);

/// Screen-space polyline width in pixels; 0 when style uses world-scaled geometry.
float gizmoPolylineWidthPx(ManipulatorAxis axis, GizmoDrawStyle style);

bool isPolylineGizmoStyle(GizmoDrawStyle style);

glm::vec4 axisColorFor(ManipulatorAxis axis, bool highlight = false);
glm::vec4 centerColor(bool highlight = false);

glm::vec3 manipulatorLocalDirection(ManipulatorAxis axis);
void manipulatorTranslationMask(ManipulatorAxis axis, bool out_mask[3]);

bool isTranslationManipulator(ManipulatorAxis axis);
bool isRotationManipulator(ManipulatorAxis axis);
bool isScaleManipulator(ManipulatorAxis axis);

/// World-space rotation axis for dial handles.
glm::vec3 rotationAxisFor(ManipulatorAxis axis, const struct GizmoBasis& basis);

}  // namespace Blunder
