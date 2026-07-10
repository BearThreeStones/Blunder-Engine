#pragma once

#include <glm/vec4.hpp>

namespace Blunder {

/// Blender `ThemeSpaceView3D` / `ED_view3d_background_color_get` default.
inline constexpr float kViewportBackgroundRgb = 0.22f;

inline glm::vec4 viewportBackgroundColor() {
  return glm::vec4(kViewportBackgroundRgb, kViewportBackgroundRgb,
                   kViewportBackgroundRgb, 1.0f);
}

}  // namespace Blunder
