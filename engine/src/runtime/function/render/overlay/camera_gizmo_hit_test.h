#pragma once

#include <cmath>
#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/function/render/overlay/camera_gizmo_geometry.h"
#include "runtime/function/scene/camera_component.h"

namespace Blunder {

enum class CameraGizmoHandleKind {
  none,
  fov_top,
  near_clip,
  far_clip,
};

/// Screen-space pick slop for Camera Gizmo wireframe (matches overlay chrome).
constexpr float kCameraGizmoPickThresholdPx = 7.0f;

/// Matches `k_origin_cross_half_len` in camera_gizmo_overlay.cpp draw path.
constexpr float kCameraGizmoOriginCrossHalfLen = 0.05f;

inline float distanceToSegment2D(const glm::vec2& point, const glm::vec2& a,
                                 const glm::vec2& b) {
  const glm::vec2 pa = point - a;
  const glm::vec2 ba = b - a;
  const float denom = glm::dot(ba, ba);
  if (denom < 1e-8f) {
    return glm::length(pa);
  }
  const float h = glm::clamp(glm::dot(pa, ba) / denom, 0.0f, 1.0f);
  return glm::length(pa - ba * h);
}

inline std::optional<glm::vec2> projectWorldToViewportLocal(
    const glm::vec3& world, const glm::mat4& view, const glm::mat4& proj,
    float viewport_width, float viewport_height) {
  const glm::vec4 clip = proj * view * glm::vec4(world, 1.0f);
  if (clip.w <= 1e-6f) {
    return std::nullopt;
  }
  const glm::vec2 ndc = glm::vec2(clip) / clip.w;
  return glm::vec2((ndc.x * 0.5f + 0.5f) * viewport_width,
                   (0.5f - ndc.y * 0.5f) * viewport_height);
}

/// Returns view-space depth of the gizmo origin when any body/frame segment hits.
inline std::optional<float> hitTestCameraGizmoFrameViewportLocal(
    const glm::vec2& pointer, const CameraGizmoFrame& frame_local,
    const glm::mat4& world_matrix, const glm::mat4& view, const glm::mat4& proj,
    float viewport_width, float viewport_height,
    float threshold_px = kCameraGizmoPickThresholdPx) {
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

  const glm::vec3 origin = to_world(frame_local.origin);
  glm::vec3 corners[4];
  for (int i = 0; i < 4; ++i) {
    corners[i] = to_world(frame_local.corners[i]);
  }
  glm::vec3 tri[3];
  for (int i = 0; i < 3; ++i) {
    tri[i] = to_world(frame_local.up_triangle[i]);
  }

  bool hit = false;
  for (int i = 0; i < 4; ++i) {
    if (test_segment(origin, corners[i])) {
      hit = true;
    }
  }
  for (int i = 0; i < 4; ++i) {
    if (test_segment(corners[i], corners[(i + 1) % 4])) {
      hit = true;
    }
  }
  for (int i = 0; i < 3; ++i) {
    if (test_segment(tri[i], tri[(i + 1) % 3])) {
      hit = true;
    }
  }

  const std::optional<glm::vec2> origin_screen = project(origin);
  if (origin_screen.has_value() &&
      glm::length(pointer - *origin_screen) <= threshold_px) {
    hit = true;
  }

  const Vec3 local_x{kCameraGizmoOriginCrossHalfLen, 0.0f, 0.0f};
  const Vec3 local_y{0.0f, kCameraGizmoOriginCrossHalfLen, 0.0f};
  if (test_segment(to_world(Vec3(-local_x.x, -local_x.y, -local_x.z)),
                   to_world(local_x))) {
    hit = true;
  }
  if (test_segment(to_world(Vec3(-local_y.x, -local_y.y, -local_y.z)),
                   to_world(local_y))) {
    hit = true;
  }

  if (!hit) {
    return std::nullopt;
  }

  return (view * glm::vec4(origin, 1.0f)).z;
}

/// Hit-test FOV / clip handles for a single selected camera. Handles win over body pick.
inline std::optional<CameraGizmoHandleKind> hitTestCameraGizmoHandlesViewportLocal(
    const glm::vec2& pointer, const CameraGizmoFrame& display_frame_local,
    const CameraComponent& camera, const glm::mat4& world_matrix,
    const glm::mat4& view, const glm::mat4& proj, float viewport_width,
    float viewport_height,
    float threshold_px = kCameraGizmoPickThresholdPx) {
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

  const float aspect = viewport_width / std::max(viewport_height, 1.0f);
  const float fov_rad = glm::radians(camera.vertical_fov_degrees);
  const CameraGizmoFrame near_frame =
      buildCameraGizmoFrameLocal(fov_rad, aspect, camera.near_clip);
  const CameraGizmoFrame far_frame =
      buildCameraGizmoFrameLocal(fov_rad, aspect, camera.far_clip);

  glm::vec3 display_corners[4];
  for (int i = 0; i < 4; ++i) {
    display_corners[i] = to_world(display_frame_local.corners[i]);
  }
  glm::vec3 up_tri[3];
  for (int i = 0; i < 3; ++i) {
    up_tri[i] = to_world(display_frame_local.up_triangle[i]);
  }

  if (test_segment(display_corners[0], display_corners[1]) ||
      test_segment(up_tri[0], up_tri[1]) || test_segment(up_tri[1], up_tri[2]) ||
      test_segment(up_tri[2], up_tri[0])) {
    return CameraGizmoHandleKind::fov_top;
  }

  glm::vec3 near_corners[4];
  for (int i = 0; i < 4; ++i) {
    near_corners[i] = to_world(near_frame.corners[i]);
  }
  glm::vec3 far_corners[4];
  for (int i = 0; i < 4; ++i) {
    far_corners[i] = to_world(far_frame.corners[i]);
  }

  bool near_hit = false;
  bool far_hit = false;
  for (int i = 0; i < 4; ++i) {
    const int next = (i + 1) % 4;
    if (test_segment(near_corners[i], near_corners[next])) {
      near_hit = true;
    }
    if (test_segment(far_corners[i], far_corners[next])) {
      far_hit = true;
    }
  }

  const glm::vec3 near_origin = to_world(near_frame.origin);
  const glm::vec3 far_origin = to_world(far_frame.origin);
  if (test_segment(to_world(display_frame_local.origin), near_origin)) {
    near_hit = true;
  }
  if (test_segment(to_world(display_frame_local.origin), far_origin)) {
    far_hit = true;
  }

  if (near_hit && far_hit) {
  const std::optional<glm::vec2> near_screen = project(near_origin);
  const std::optional<glm::vec2> far_screen = project(far_origin);
    if (near_screen.has_value() && far_screen.has_value()) {
      const float near_dist = glm::length(pointer - *near_screen);
      const float far_dist = glm::length(pointer - *far_screen);
      return near_dist <= far_dist ? CameraGizmoHandleKind::near_clip
                                   : CameraGizmoHandleKind::far_clip;
    }
    return CameraGizmoHandleKind::near_clip;
  }
  if (near_hit) {
    return CameraGizmoHandleKind::near_clip;
  }
  if (far_hit) {
    return CameraGizmoHandleKind::far_clip;
  }

  return std::nullopt;
}

}  // namespace Blunder
