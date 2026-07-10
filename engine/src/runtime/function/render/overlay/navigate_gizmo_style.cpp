#include "runtime/function/render/overlay/navigate_gizmo_style.h"

#include <algorithm>

#include "runtime/function/render/overlay/navigate_gizmo_shared.h"

namespace Blunder {

glm::vec4 navigateAxisFadingColor(const glm::vec4& view_bg, const glm::vec3& axis_rgb,
                                  const float depth) {
  const float t = (depth + 1.0f) * 0.25f + 0.5f;
  const glm::vec4 axis{axis_rgb, 1.0f};
  return glm::mix(view_bg, axis, t);
}

glm::vec4 navigateAxisMiddleColor(const glm::vec4& view_bg, const glm::vec3& axis_rgb) {
  const glm::vec4 axis{axis_rgb, 1.0f};
  return glm::mix(view_bg, axis, 0.75f);
}

glm::vec4 navigateAxisNegativeColor(const glm::vec3& axis_rgb) {
  const glm::vec3 gray(NavigateGizmoMetrics::kNegBallFillGray);
  const glm::vec3 fill =
      glm::mix(gray, axis_rgb, NavigateGizmoMetrics::kNegBallFillMix);
  return glm::vec4(fill, 1.0f);
}

glm::vec4 navigateGizmoHighlightBackdropColor() {
  return glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);
}

}  // namespace Blunder
