#include "runtime/function/render/gizmo/transform_gizmo_types.h"

#include <algorithm>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/render/gizmo/gizmo_math.h"

namespace Blunder {

namespace {

/// Blender TH_GIZMO_VIEW_ALIGN (default theme: white).
const glm::vec3 kGizmoViewAlignColor{1.0f, 1.0f, 1.0f};

glm::vec3 themeAxisRgb(const ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::trans_x:
    case ManipulatorAxis::rot_x:
      return kAxisColorPositiveX;
    case ManipulatorAxis::trans_y:
    case ManipulatorAxis::rot_y:
      return kAxisColorPositiveY;
    case ManipulatorAxis::trans_z:
    case ManipulatorAxis::rot_z:
      return kAxisColorPositiveZ;
    case ManipulatorAxis::trans_yz:
      return kAxisColorPositiveX;
    case ManipulatorAxis::trans_zx:
      return kAxisColorPositiveY;
    case ManipulatorAxis::trans_xy:
      return kAxisColorPositiveZ;
    case ManipulatorAxis::trans_c:
    case ManipulatorAxis::rot_t:
    case ManipulatorAxis::rot_c:
    case ManipulatorAxis::scale_c_outer:
      return kGizmoViewAlignColor;
    default:
      return kGizmoViewAlignColor;
  }
}

}  // namespace

bool isTranslationManipulator(const ManipulatorAxis axis) {
  return axis >= ManipulatorAxis::trans_x && axis <= ManipulatorAxis::trans_c;
}

bool isRotationManipulator(const ManipulatorAxis axis) {
  return axis >= ManipulatorAxis::rot_x && axis <= ManipulatorAxis::rot_z;
}

bool isScaleManipulator(const ManipulatorAxis axis) {
  return axis >= ManipulatorAxis::trans_x && axis <= ManipulatorAxis::trans_c;
}

bool isPlaneManipulator(const ManipulatorAxis axis) {
  return axis == ManipulatorAxis::trans_xy || axis == ManipulatorAxis::trans_yz ||
         axis == ManipulatorAxis::trans_zx;
}

uint32_t gizmoOrientationAxis(const ManipulatorAxis axis, bool* out_is_plane) {
  if (out_is_plane) {
    *out_is_plane = isPlaneManipulator(axis);
  }
  switch (axis) {
    case ManipulatorAxis::trans_yz:
      return 0;
    case ManipulatorAxis::trans_zx:
      return 1;
    case ManipulatorAxis::trans_xy:
      return 2;
    case ManipulatorAxis::trans_x:
    case ManipulatorAxis::rot_x:
      return 0;
    case ManipulatorAxis::trans_y:
    case ManipulatorAxis::rot_y:
      return 1;
    case ManipulatorAxis::trans_z:
    case ManipulatorAxis::rot_z:
      return 2;
    default:
      return 3;
  }
}

float gizmoAxisFadeFactor(const ManipulatorAxis axis, const float idot[3]) {
  if (axis == ManipulatorAxis::rot_t || axis == ManipulatorAxis::rot_c ||
      axis == ManipulatorAxis::scale_c_outer) {
    return 1.0f;
  }

  if (isRotationManipulator(axis)) {
    return 1.0f;
  }

  bool is_plane = false;
  const uint32_t axis_norm = gizmoOrientationAxis(axis, &is_plane);
  if (axis_norm >= 3) {
    return 1.0f;
  }

  float idot_axis = idot[axis_norm];
  if (is_plane) {
    idot_axis = 1.0f - idot_axis;
  }

  const float fade_min = is_plane ? TransformGizmoMetrics::k_plane_idot_fade_min
                                  : TransformGizmoMetrics::k_axis_idot_fade_min;
  const float fade_max = is_plane ? TransformGizmoMetrics::k_plane_idot_fade_max
                                  : TransformGizmoMetrics::k_axis_idot_fade_max;
  if (idot_axis < fade_min) {
    return 0.0f;
  }
  if (idot_axis > fade_max) {
    return 1.0f;
  }
  return (idot_axis - fade_min) / std::max(fade_max - fade_min, 1e-4f);
}

GizmoAxisColor gizmoAxisColor(const ManipulatorAxis axis, const float idot[3]) {
  const glm::vec3 rgb = themeAxisRgb(axis);
  float alpha_fac = gizmoAxisFadeFactor(axis, idot);
  if (axis == ManipulatorAxis::rot_t) {
    // Blender MAN_AXIS_ROT_T: faint trackball overlay (~5% of axis alpha).
    alpha_fac = TransformGizmoMetrics::k_trackball_alpha_factor;
  }
  GizmoAxisColor result{};
  result.color =
      glm::vec4(rgb, TransformGizmoMetrics::k_gizmo_color_alpha * alpha_fac);
  result.color_hi =
      glm::vec4(rgb, TransformGizmoMetrics::k_gizmo_color_alpha_hi * alpha_fac);
  return result;
}

bool gizmoHandleUsesHoverColorHi(const ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::rot_t:
    case ManipulatorAxis::rot_c:
    case ManipulatorAxis::scale_c_outer:
      return false;
    default:
      return true;
  }
}

glm::vec4 gizmoColorGet(const ManipulatorAxis axis, const float idot[3],
                        const bool highlight) {
  const GizmoAxisColor colors = gizmoAxisColor(axis, idot);
  return (highlight && gizmoHandleUsesHoverColorHi(axis)) ? colors.color_hi : colors.color;
}

float gizmoLineWidthScale(const ManipulatorAxis axis, const GizmoDrawStyle style) {
  if (isPolylineGizmoStyle(style)) {
    return 1.0f;
  }
  return 1.0f;
}

bool isPolylineGizmoStyle(const GizmoDrawStyle style) {
  return style == GizmoDrawStyle::dial || style == GizmoDrawStyle::dial_ghost ||
         style == GizmoDrawStyle::dial_wire || style == GizmoDrawStyle::annulus;
}

float gizmoSdfStrokeWidthPx(const GizmoDrawStyle style) {
  switch (style) {
    case GizmoDrawStyle::arrow:
    case GizmoDrawStyle::plane:
    case GizmoDrawStyle::scale_stem_box:
      return TransformGizmoMetrics::k_sdf_stroke_width_px;
    default:
      return 0.0f;
  }
}

float gizmoPolylineWidthPx(const ManipulatorAxis axis, const GizmoDrawStyle style) {
  if (const float sdf_width = gizmoSdfStrokeWidthPx(style); sdf_width > 0.0f) {
    return sdf_width;
  }
  if (!isPolylineGizmoStyle(style)) {
    return 0.0f;
  }
  if (style == GizmoDrawStyle::dial_wire || style == GizmoDrawStyle::annulus) {
    return TransformGizmoMetrics::k_outer_ring_polyline_width_px;
  }
  if (isRotationManipulator(axis) || style == GizmoDrawStyle::dial ||
      style == GizmoDrawStyle::dial_ghost) {
    return TransformGizmoMetrics::k_dial_polyline_width_px;
  }
  return TransformGizmoMetrics::k_dial_polyline_width_px;
}

glm::vec4 axisColorFor(const ManipulatorAxis axis, const bool highlight) {
  const float idot[3] = {1.0f, 1.0f, 1.0f};
  return gizmoColorGet(axis, idot, highlight);
}

glm::vec4 centerColor(const bool highlight) {
  const float idot[3] = {1.0f, 1.0f, 1.0f};
  return gizmoColorGet(ManipulatorAxis::trans_c, idot, highlight);
}

glm::vec3 manipulatorLocalDirection(const ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::trans_x:
      return glm::vec3(1.0f, 0.0f, 0.0f);
    case ManipulatorAxis::trans_y:
      return glm::vec3(0.0f, 1.0f, 0.0f);
    case ManipulatorAxis::trans_z:
      return glm::vec3(0.0f, 0.0f, 1.0f);
    default:
      return glm::vec3(0.0f);
  }
}

void manipulatorTranslationMask(const ManipulatorAxis axis, bool out_mask[3]) {
  out_mask[0] = out_mask[1] = out_mask[2] = false;
  switch (axis) {
    case ManipulatorAxis::trans_x:
      out_mask[0] = true;
      break;
    case ManipulatorAxis::trans_y:
      out_mask[1] = true;
      break;
    case ManipulatorAxis::trans_z:
      out_mask[2] = true;
      break;
    case ManipulatorAxis::trans_xy:
      out_mask[0] = out_mask[1] = true;
      break;
    case ManipulatorAxis::trans_yz:
      out_mask[1] = out_mask[2] = true;
      break;
    case ManipulatorAxis::trans_zx:
      out_mask[2] = out_mask[0] = true;
      break;
    case ManipulatorAxis::trans_c:
      out_mask[0] = out_mask[1] = out_mask[2] = true;
      break;
    default:
      break;
  }
}

float gizmoScaleBasisForAxis(const ManipulatorAxis axis) {
  if (axis == ManipulatorAxis::trans_c) {
    return TransformGizmoMetrics::k_translate_center_scale_basis;
  }
  if (axis == ManipulatorAxis::rot_c) {
    return TransformGizmoMetrics::k_rotate_outer_ring_scale_basis;
  }
  if (axis == ManipulatorAxis::scale_c_outer) {
    return TransformGizmoMetrics::k_scale_outer_ring_scale_basis;
  }
  if (isRotationManipulator(axis)) {
    return TransformGizmoMetrics::k_rotate_axis_dial_scale_basis;
  }
  return TransformGizmoMetrics::k_translate_handle_scale_basis;
}

glm::vec3 rotationAxisFor(const ManipulatorAxis axis, const GizmoBasis& basis) {
  switch (axis) {
    case ManipulatorAxis::rot_x:
      return basis.axis_x;
    case ManipulatorAxis::rot_y:
      return basis.axis_y;
    case ManipulatorAxis::rot_z:
      return basis.axis_z;
    default:
      return basis.axis_z;
  }
}

}  // namespace Blunder
