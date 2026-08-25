#pragma once

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/function/render/overlay/camera_gizmo_hit_test.h"
#include "runtime/function/render/overlay/light_gizmo_geometry.h"

namespace Blunder {

constexpr float kLightGizmoPickThresholdPx = 7.0f;

inline std::optional<float> hitTestLightGizmoViewportLocal(
    const glm::vec2& pointer, const LightGizmoShape& shape,
    const glm::mat4& world_matrix, const glm::mat4& view, const glm::mat4& proj,
    float viewport_width, float viewport_height,
    float threshold_px = kLightGizmoPickThresholdPx) {
  const auto to_world = [&](const Vec3& local) -> glm::vec3 {
    return glm::vec3(world_matrix * glm::vec4(local.x, local.y, local.z, 1.0f));
  };
  const auto project = [&](const glm::vec3& world) -> std::optional<glm::vec2> {
    return projectWorldToViewportLocal(world, view, proj, viewport_width,
                                       viewport_height);
  };
  const auto test_segment = [&](const glm::vec3& wa, const glm::vec3& wb) -> bool {
    const std::optional<glm::vec2> sa = project(wa);
    const std::optional<glm::vec2> sb = project(wb);
    if (!sa.has_value() || !sb.has_value()) {
      return false;
    }
    return distanceToSegment2D(pointer, *sa, *sb) <= threshold_px;
  };

  bool hit = false;
  forEachLightGizmoSegmentLocal(shape, [&](const Vec3& a, const Vec3& b) {
    if (test_segment(to_world(a), to_world(b))) {
      hit = true;
    }
  });
  if (!hit) {
    return std::nullopt;
  }
  const glm::vec3 origin = to_world(Vec3(0.0f));
  return (view * glm::vec4(origin, 1.0f)).z;
}

}  // namespace Blunder
