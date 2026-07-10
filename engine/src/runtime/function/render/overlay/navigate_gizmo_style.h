#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Blunder {

/// Blender DNA_userdef_types.h `gizmo_size_navigate_v3d` default.
constexpr float kBlenderNavigateGizmoDiameterPx = 80.0f;
constexpr float kBlenderNavigateGizmoOffsetPx = 10.0f;

inline float navigateGizmoDiameterPx(const float ui_scale) {
  return kBlenderNavigateGizmoDiameterPx * ui_scale;
}

/// Blender view3d_gizmo_navigate_type.cc — depth-weighted axis colors.
glm::vec4 navigateAxisFadingColor(const glm::vec4& view_bg, const glm::vec3& axis_rgb,
                                  float depth);
glm::vec4 navigateAxisMiddleColor(const glm::vec4& view_bg, const glm::vec3& axis_rgb);
/// Opaque muted axis fill for negative balls (border uses full axis color in the shader).
glm::vec4 navigateAxisNegativeColor(const glm::vec3& axis_rgb);

/// Blender view3d_gizmo_navigate.cc rotate gizmo highlight backdrop.
glm::vec4 navigateGizmoHighlightBackdropColor();

}  // namespace Blunder
