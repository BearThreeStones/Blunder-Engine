#pragma once

#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"

namespace Blunder {

/// Clip NDC for a pointer in viewport-local pixels (origin = top-left).
inline Vec2 viewportLocalToClipNdc(const Vec2& local, float width, float height) {
  if (width <= 0.0f || height <= 0.0f) {
    return Vec2(0.0f, 0.0f);
  }
  const float normalized_x = local.x / width;
  const float normalized_y = local.y / height;
  // Vulkan framebuffer row 0 is the top of the image (NDC y = -1). Pointer
  // Y=0 is that same top — OpenGL's (1 - 2*ny) would invert ground follow.
  return Vec2(normalized_x * 2.0f - 1.0f, normalized_y * 2.0f - 1.0f);
}

/// Slint logical window position → viewport-local pixels (no HiDPI remap).
inline Vec2 logicalToViewportLocal(const Vec2& logical, const Vec2& origin) {
  return Vec2(logical.x - origin.x, logical.y - origin.y);
}

inline bool isLogicalPositionInViewport(const Vec2& logical, const Vec2& origin,
                                        float logical_width, float logical_height) {
  if (logical_width <= 0.0f || logical_height <= 0.0f) {
    return true;
  }
  return logical.x >= origin.x && logical.x <= origin.x + logical_width &&
         logical.y >= origin.y && logical.y <= origin.y + logical_height;
}

inline Ray unprojectViewportRay(const Vec2& ndc, const Mat4& view,
                                const Mat4& projection,
                                const Vec3& camera_position, bool orthographic) {
  const Mat4 inverse_view_projection = glm::inverse(projection * view);
  Vec4 near_world = inverse_view_projection * Vec4(ndc.x, ndc.y, 0.0f, 1.0f);
  Vec4 far_world = inverse_view_projection * Vec4(ndc.x, ndc.y, 1.0f, 1.0f);
  near_world /= near_world.w;
  far_world /= far_world.w;

  const Vec3 near_point(near_world.x, near_world.y, near_world.z);
  const Vec3 far_point(far_world.x, far_world.y, far_world.z);
  const Vec3 direction = glm::normalize(far_point - near_point);
  if (orthographic) {
    return Ray{near_point, direction};
  }
  return Ray{camera_position, direction};
}

}  // namespace Blunder
