#pragma once

#include <cmath>
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/function/render/overlay/navigate_gizmo_layout.h"
#include "runtime/function/render/overlay/navigate_gizmo_shared.h"

namespace Blunder {

/// Axis endpoint index: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z.
struct NavigateGizmoAxisHit {
  int endpoint_index{-1};
};

inline glm::vec2 viewportLocalToGizmoPos2D(float viewport_x, float viewport_y,
                                           const Rect2f& gizmo_rect) {
  const float u = (viewport_x - gizmo_rect.x) / gizmo_rect.width;
  const float v = (viewport_y - gizmo_rect.y) / gizmo_rect.height;
  glm::vec2 pos2d;
  pos2d.x = (u * 2.0f - 1.0f) * NavigateGizmoMetrics::kBgRadius;
  pos2d.y = (1.0f - v * 2.0f) * NavigateGizmoMetrics::kBgRadius;
  return pos2d;
}

inline glm::vec3 gizmoEndpointWorldDirection(int endpoint_index) {
  const int axis_index = endpoint_index / 2;
  const bool is_positive = (endpoint_index % 2) == 0;
  glm::vec3 axis(0.0f);
  axis[axis_index] = is_positive ? 1.0f : -1.0f;
  return axis;
}

inline glm::vec2 worldAxisToGizmoViewDir2D(const glm::mat4& view,
                                           const glm::vec3& world_direction) {
  const glm::vec4 view_dir = view * glm::vec4(world_direction, 0.0f);
  return glm::vec2(view_dir.x, view_dir.y);
}

inline float endpointViewDepth(const glm::mat4& view, int endpoint_index) {
  const int axis_index = endpoint_index / 2;
  const bool is_positive = (endpoint_index % 2) == 0;
  const float axis_depth = view[axis_index][2];
  return is_positive ? axis_depth : -axis_depth;
}

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

inline glm::vec2 projectGizmoPoint(const glm::vec3& model_pos, const glm::mat4& view_rotation, float perspective_factor, float D, float& out_visual_scale) {
  glm::vec3 view_pos = glm::vec3(view_rotation * glm::vec4(model_pos, 0.0f));
  view_pos.z -= D;
  out_visual_scale = glm::mix(1.0f, D / -view_pos.z, perspective_factor);
  return glm::vec2(view_pos.x, view_pos.y) * out_visual_scale;
}

/// Picks the front-most axis handle under the cursor (balls and arms).
inline std::optional<NavigateGizmoAxisHit> hitTestNavigateGizmoAxis(
    float viewport_local_x, float viewport_local_y,
    const NavigateGizmoLayout& layout, const glm::mat4& view,
    float perspective_factor) {
  if (!layout.visible ||
      !rectContains(layout.gizmo_rect, viewport_local_x, viewport_local_y)) {
    return std::nullopt;
  }

  const glm::vec2 pos2d = viewportLocalToGizmoPos2D(
      viewport_local_x, viewport_local_y, layout.gizmo_rect);

  int best_endpoint = -1;
  float best_depth = -1e9f;

  constexpr float k_D = 2.6556443f;
  glm::mat4 view_rotation = view;
  view_rotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

  for (int endpoint = 0; endpoint < 6; ++endpoint) {
    const glm::vec3 world_dir = gizmoEndpointWorldDirection(endpoint);
    
    float out_visual_scale = 1.0f;
    const glm::vec2 end_pos = projectGizmoPoint(world_dir * NavigateGizmoMetrics::kArmLength,
                                                 view_rotation, perspective_factor, k_D, out_visual_scale);
                                                
    const bool is_positive = (endpoint % 2) == 0;
    const float ball_radius =
        (is_positive ? NavigateGizmoMetrics::kBallRadius
                     : NavigateGizmoMetrics::kNegBallRadius) *
        NavigateGizmoMetrics::kHitSlop * out_visual_scale;

    bool hit = false;
    if (glm::length(pos2d - end_pos) <= ball_radius) {
      hit = true;
    } else {
      float center_scale = 1.0f;
      const glm::vec2 arm_start = projectGizmoPoint(world_dir * (NavigateGizmoMetrics::kCenterRadius * 0.5f),
                                                     view_rotation, perspective_factor, k_D, center_scale);
      const float arm_hit_radius =
          NavigateGizmoMetrics::kArmHalfWidth * NavigateGizmoMetrics::kHitSlop *
          2.5f * (out_visual_scale + center_scale) * 0.5f;
      if (distanceToSegment2D(pos2d, arm_start, end_pos) <= arm_hit_radius) {
        hit = true;
      }
    }

    if (!hit) {
      continue;
    }

    glm::vec3 model_pos = world_dir * NavigateGizmoMetrics::kArmLength;
    glm::vec3 view_pos = glm::vec3(view_rotation * glm::vec4(model_pos, 0.0f));
    const float depth = view_pos.z;

    if (depth > best_depth) {
      best_depth = depth;
      best_endpoint = endpoint;
    }
  }

  if (best_endpoint < 0) {
    return std::nullopt;
  }
  return NavigateGizmoAxisHit{best_endpoint};
}

}  // namespace Blunder
