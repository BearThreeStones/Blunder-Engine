#pragma once

#include <optional>

#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"
#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"

namespace Blunder {

struct TransformGizmoPickContext {
  Ray ray;
  GizmoBasis basis;
  /// Blender wm_gizmo_calculate_scale group factor (before scale_basis).
  float group_scale{1.0f};
  /// ED_view3d_pixel_size_no_ui_scale at pivot (world units per pixel).
  float gizmo_pixel_size{1.0f};
  glm::vec3 camera_forward{0.0f, 0.0f, -1.0f};
  glm::vec3 camera_position{0.0f};
};

std::optional<ManipulatorAxis> pickTranslationGizmoHandle(
    const TransformGizmoPickContext& ctx);

std::optional<ManipulatorAxis> pickRotationGizmoHandle(
    const TransformGizmoPickContext& ctx);

std::optional<ManipulatorAxis> pickScaleGizmoHandle(
    const TransformGizmoPickContext& ctx);

bool gizmoHandleHighlighted(std::optional<ManipulatorAxis> active_axis,
                            std::optional<ManipulatorAxis> hover_axis,
                            ManipulatorAxis axis);

std::optional<ManipulatorAxis> pickTransformGizmoHandle(
    TransformGizmoMode mode, const TransformGizmoPickContext& ctx);

}  // namespace Blunder
