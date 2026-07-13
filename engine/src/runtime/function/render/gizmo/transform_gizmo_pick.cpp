#include "runtime/function/render/gizmo/transform_gizmo_pick.h"

#include <cmath>

#include "EASTL/vector.h"

namespace Blunder {

namespace {

float pickThreshold(const float handle_scale) {
  return handle_scale * TransformGizmoMetrics::k_mesh_stem_radius *
         TransformGizmoMetrics::k_pick_slop * 14.0f;
}

bool pickAxis(const Ray& ray, const glm::vec3& origin, const glm::vec3& direction,
              float length, float threshold, float& out_distance) {
  const glm::vec3 end = origin + glm::normalize(direction) * length;
  const float dist = distanceRayToSegment(ray, origin, end);
  if (dist > threshold) {
    return false;
  }
  out_distance = dist;
  return true;
}

bool pickPlaneHandle(const Ray& ray, const glm::vec3& center, const glm::vec3& axis_u,
                     const glm::vec3& axis_v, float half_extent, float threshold,
                     float& out_distance) {
  // Expand the pick quad slightly so the visible plane is easy to hit.
  const float pick_half = half_extent + threshold;
  const auto hit =
      intersectRayPlaneQuad(ray, center, axis_u, axis_v, pick_half);
  if (!hit) {
    return false;
  }
  // Perpendicular distance from the ray to the hit is ~0 for a true ray hit.
  // That beats nearby axis segment distances so corner planes are selectable.
  const glm::vec3 w = *hit - ray.origin;
  out_distance = glm::length(glm::cross(ray.direction, w)) /
                 std::max(glm::length(ray.direction), 1e-6f);
  return true;
}

bool pickCenter(const Ray& ray, const glm::vec3& center, float radius, float threshold,
                float& out_distance) {
  const glm::vec3 oc = ray.origin - center;
  const float b = glm::dot(oc, ray.direction);
  const float c = glm::dot(oc, oc) - radius * radius;
  const float disc = b * b - c;
  if (disc < 0.0f) {
    return false;
  }
  const float t = -b - std::sqrt(disc);
  if (t < 0.0f) {
    return false;
  }
  out_distance = t;
  return t < radius + threshold;
}

bool pickDialRing(const Ray& ray, const glm::vec3& pivot, const glm::vec3& axis,
                  float major_radius, float tube_radius, float& out_distance) {
  const auto hit = intersectRayWithAxisPlane(ray, pivot, axis);
  if (!hit) {
    return false;
  }
  const glm::vec3 radial = *hit - pivot;
  const float dist = glm::length(radial);
  const float threshold = tube_radius * TransformGizmoMetrics::k_pick_slop * 4.0f;
  if (std::abs(dist - major_radius) > threshold) {
    return false;
  }
  out_distance = glm::length(*hit - ray.origin);
  return true;
}

ManipulatorAxis pickBest(eastl::vector<eastl::pair<ManipulatorAxis, float>>& hits) {
  ManipulatorAxis best = hits.front().first;
  float best_dist = hits.front().second;
  for (size_t i = 1; i < hits.size(); ++i) {
    if (hits[i].second < best_dist) {
      best_dist = hits[i].second;
      best = hits[i].first;
    }
  }
  return best;
}

ManipulatorAxis pickBestTranslation(
    eastl::vector<eastl::pair<ManipulatorAxis, float>>& hits) {
  // If the ray hit a plane quad, prefer that over nearby axes/center. Plane
  // handles sit in axis corners where axis thresholds always overlap.
  bool has_plane = false;
  ManipulatorAxis best_plane = ManipulatorAxis::last;
  float best_plane_dist = 1e10f;
  for (const auto& hit : hits) {
    if (!isPlaneManipulator(hit.first)) {
      continue;
    }
    has_plane = true;
    if (hit.second < best_plane_dist) {
      best_plane_dist = hit.second;
      best_plane = hit.first;
    }
  }
  if (has_plane) {
    return best_plane;
  }
  return pickBest(hits);
}

}  // namespace

std::optional<ManipulatorAxis> pickTranslationGizmoHandle(
    const TransformGizmoPickContext& ctx) {
  const float arrow_scale =
      computeGizmoHandleScale(ctx.group_scale, ManipulatorAxis::trans_x);
  const float plane_scale =
      computeGizmoHandleScale(ctx.group_scale, ManipulatorAxis::trans_xy);
  const float center_scale =
      computeGizmoHandleScale(ctx.group_scale, ManipulatorAxis::trans_c);

  const float axis_len = arrow_scale * TransformGizmoMetrics::k_mesh_arrow_length;
  const float threshold = pickThreshold(arrow_scale);
  const float plane_half = plane_scale * TransformGizmoMetrics::k_mesh_plane_half_extent;
  const float center_r = center_scale * TransformGizmoMetrics::k_mesh_center_radius;

  eastl::vector<eastl::pair<ManipulatorAxis, float>> hits;
  hits.reserve(7);

  auto try_axis = [&](ManipulatorAxis axis, const glm::vec3& dir) {
    float dist = 0.0f;
    if (pickAxis(ctx.ray, ctx.basis.origin, dir, axis_len, threshold, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_axis(ManipulatorAxis::trans_x, ctx.basis.axis_x);
  try_axis(ManipulatorAxis::trans_y, ctx.basis.axis_y);
  try_axis(ManipulatorAxis::trans_z, ctx.basis.axis_z);

  const float plane_offset =
      plane_scale * TransformGizmoMetrics::k_mesh_plane_center_offset;
  auto try_plane = [&](ManipulatorAxis axis, const glm::vec3& u, const glm::vec3& v) {
    const glm::vec3 center = ctx.basis.origin + glm::normalize(u) * plane_offset +
                             glm::normalize(v) * plane_offset;
    float dist = 0.0f;
    if (pickPlaneHandle(ctx.ray, center, u, v, plane_half, threshold, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_plane(ManipulatorAxis::trans_xy, ctx.basis.axis_x, ctx.basis.axis_y);
  try_plane(ManipulatorAxis::trans_yz, ctx.basis.axis_y, ctx.basis.axis_z);
  try_plane(ManipulatorAxis::trans_zx, ctx.basis.axis_z, ctx.basis.axis_x);

  {
    float dist = 0.0f;
    if (pickCenter(ctx.ray, ctx.basis.origin, center_r, threshold, dist)) {
      hits.push_back({ManipulatorAxis::trans_c, dist});
    }
  }

  if (hits.empty()) {
    return std::nullopt;
  }
  return pickBestTranslation(hits);
}

std::optional<ManipulatorAxis> pickRotationGizmoHandle(
    const TransformGizmoPickContext& ctx) {
  const float dial_scale =
      computeGizmoHandleScale(ctx.group_scale, ManipulatorAxis::rot_x);
  const float major_r = dial_scale * TransformGizmoMetrics::k_mesh_dial_major_radius;
  const float tube_r = ctx.gizmo_pixel_size *
                       TransformGizmoMetrics::k_dial_polyline_width_px * 0.5f *
                       TransformGizmoMetrics::k_pick_slop;

  eastl::vector<eastl::pair<ManipulatorAxis, float>> hits;
  hits.reserve(3);

  auto try_dial = [&](ManipulatorAxis axis, const glm::vec3& rot_axis) {
    float dist = 0.0f;
    if (pickDialRing(ctx.ray, ctx.basis.origin, rot_axis, major_r, tube_r, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_dial(ManipulatorAxis::rot_x, ctx.basis.axis_x);
  try_dial(ManipulatorAxis::rot_y, ctx.basis.axis_y);
  try_dial(ManipulatorAxis::rot_z, ctx.basis.axis_z);

  if (hits.empty()) {
    return std::nullopt;
  }
  return pickBest(hits);
}

std::optional<ManipulatorAxis> pickScaleGizmoHandle(
    const TransformGizmoPickContext& ctx) {
  const float axis_scale =
      computeGizmoHandleScale(ctx.group_scale, ManipulatorAxis::trans_x);
  const float plane_scale =
      computeGizmoHandleScale(ctx.group_scale, ManipulatorAxis::trans_xy);
  const float center_scale =
      computeGizmoHandleScale(ctx.group_scale, ManipulatorAxis::trans_c);
  const float axis_half =
      axis_scale * TransformGizmoMetrics::k_mesh_scale_box_half_extent;
  const float center_half =
      center_scale * TransformGizmoMetrics::k_mesh_scale_center_half_extent;
  const float threshold = pickThreshold(axis_scale);
  const float stem_start =
      axis_scale * TransformGizmoMetrics::k_mesh_scale_stem_start;
  const float stem_end =
      axis_scale * TransformGizmoMetrics::k_mesh_scale_box_center_offset;
  const float plane_half =
      plane_scale * TransformGizmoMetrics::k_mesh_plane_half_extent *
      TransformGizmoMetrics::k_scale_plane_length_factor;
  const float plane_offset =
      plane_scale * TransformGizmoMetrics::k_mesh_plane_center_offset;

  eastl::vector<eastl::pair<ManipulatorAxis, float>> hits;
  hits.reserve(7);

  auto try_stem = [&](const ManipulatorAxis axis, const glm::vec3& dir) {
    const glm::vec3 start = ctx.basis.origin + glm::normalize(dir) * stem_start;
    const glm::vec3 end = ctx.basis.origin + glm::normalize(dir) * stem_end;
    float dist = 0.0f;
    if (pickAxis(ctx.ray, start, end - start, glm::length(end - start), threshold, dist)) {
      hits.push_back({axis, dist});
    }
  };

  auto try_box = [&](const ManipulatorAxis axis, const glm::vec3& center,
                     const float half_extent) {
    float dist = 0.0f;
    if (pickCenter(ctx.ray, center, half_extent * 1.2f, threshold, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_stem(ManipulatorAxis::trans_x, ctx.basis.axis_x);
  try_stem(ManipulatorAxis::trans_y, ctx.basis.axis_y);
  try_stem(ManipulatorAxis::trans_z, ctx.basis.axis_z);

  try_box(ManipulatorAxis::trans_x,
          scaleHandleWorldCenter(ctx.basis, ManipulatorAxis::trans_x, axis_scale),
          axis_half);
  try_box(ManipulatorAxis::trans_y,
          scaleHandleWorldCenter(ctx.basis, ManipulatorAxis::trans_y, axis_scale),
          axis_half);
  try_box(ManipulatorAxis::trans_z,
          scaleHandleWorldCenter(ctx.basis, ManipulatorAxis::trans_z, axis_scale),
          axis_half);

  auto try_plane = [&](const ManipulatorAxis axis, const glm::vec3& u, const glm::vec3& v) {
    const glm::vec3 center = ctx.basis.origin + glm::normalize(u) * plane_offset +
                             glm::normalize(v) * plane_offset;
    float dist = 0.0f;
    if (pickPlaneHandle(ctx.ray, center, u, v, plane_half, threshold, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_plane(ManipulatorAxis::trans_xy, ctx.basis.axis_x, ctx.basis.axis_y);
  try_plane(ManipulatorAxis::trans_yz, ctx.basis.axis_y, ctx.basis.axis_z);
  try_plane(ManipulatorAxis::trans_zx, ctx.basis.axis_z, ctx.basis.axis_x);

  try_box(ManipulatorAxis::trans_c, ctx.basis.origin, center_half);

  if (hits.empty()) {
    return std::nullopt;
  }
  return pickBest(hits);
}

bool gizmoHandleHighlighted(const std::optional<ManipulatorAxis> active_axis,
                            const std::optional<ManipulatorAxis> hover_axis,
                            const ManipulatorAxis axis) {
  if (active_axis) {
    return *active_axis == axis;
  }
  return hover_axis && *hover_axis == axis;
}

std::optional<ManipulatorAxis> pickTransformGizmoHandle(
    const TransformGizmoMode mode, const TransformGizmoPickContext& ctx) {
  switch (mode) {
    case TransformGizmoMode::translate:
      return pickTranslationGizmoHandle(ctx);
    case TransformGizmoMode::rotate:
      return pickRotationGizmoHandle(ctx);
    case TransformGizmoMode::scale:
      return pickScaleGizmoHandle(ctx);
    case TransformGizmoMode::none:
    default:
      return std::nullopt;
  }
}

}  // namespace Blunder
