#include "runtime/function/render/gizmo/transform_gizmo_pick.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "EASTL/vector.h"

namespace Blunder {

namespace {

float pickWorldSlop(const float handle_scale, const float pixel_size,
                    const float min_pixels) {
  using M = TransformGizmoMetrics;
  const float from_mesh = handle_scale * M::k_mesh_stem_radius * M::k_pick_slop;
  const float from_px =
      std::max(pixel_size, 0.0f) * min_pixels * M::k_pick_slop;
  return std::max(from_mesh, from_px);
}

float pixelSizeAt(const TransformGizmoPickContext& ctx, const glm::vec3& point) {
  const float d_pivot = glm::length(ctx.camera_position - ctx.basis.origin);
  if (d_pivot < 1e-4f) {
    return ctx.gizmo_pixel_size;
  }
  const float d_point = glm::length(ctx.camera_position - point);
  return ctx.gizmo_pixel_size * (d_point / d_pivot);
}

bool handlePickable(const TransformGizmoPickContext& ctx, const ManipulatorAxis axis) {
  const glm::vec3 to_cam = ctx.camera_position - ctx.basis.origin;
  if (glm::length(to_cam) < 1e-4f) {
    return true;
  }
  float idot[3]{};
  computeGizmoIdot(ctx.basis, ctx.camera_position, ctx.basis.origin, idot);
  return gizmoAxisFadeFactor(axis, idot) > 0.0f;
}

float rayClosestDistance(const Ray& ray, const glm::vec3& point) {
  return glm::length(glm::cross(ray.direction, point - ray.origin)) /
         std::max(glm::length(ray.direction), 1e-6f);
}

bool pickAxis(const Ray& ray, const glm::vec3& origin, const glm::vec3& direction,
              float length, float threshold, float& out_distance) {
  if (length <= 1e-6f) {
    return false;
  }
  const glm::vec3 end = origin + glm::normalize(direction) * length;
  const float dist = distanceRayToSegment(ray, origin, end);
  if (dist > threshold) {
    return false;
  }
  out_distance = dist;
  return true;
}

float axisPickSlop(const TransformGizmoPickContext& ctx, const float handle_scale,
                   const glm::vec3& a, const glm::vec3& b) {
  using M = TransformGizmoMetrics;
  const float slop_a =
      pickWorldSlop(handle_scale, pixelSizeAt(ctx, a), M::k_axis_pick_pixels);
  const float slop_b =
      pickWorldSlop(handle_scale, pixelSizeAt(ctx, b), M::k_axis_pick_pixels);
  return std::max(slop_a, slop_b);
}

bool pickPlaneHandle(const Ray& ray, const glm::vec3& center, const glm::vec3& axis_u,
                     const glm::vec3& axis_v, float half_extent, float threshold,
                     float& out_distance) {
  const float pick_half = half_extent + threshold;
  const auto hit =
      intersectRayPlaneQuad(ray, center, axis_u, axis_v, pick_half);
  if (!hit) {
    return false;
  }
  out_distance = rayClosestDistance(ray, *hit);
  return true;
}

bool pickCenter(const Ray& ray, const glm::vec3& center, float radius, float threshold,
                float& out_distance) {
  const float pick_radius = radius + threshold;
  const float dir_len = glm::length(ray.direction);
  if (dir_len < 1e-6f || pick_radius <= 0.0f) {
    return false;
  }
  const glm::vec3 dir = ray.direction / dir_len;
  const glm::vec3 oc = ray.origin - center;
  const float b = glm::dot(oc, dir);
  const float c = glm::dot(oc, oc) - pick_radius * pick_radius;
  const float disc = b * b - c;
  if (disc < 0.0f) {
    return false;
  }
  const float sqrt_disc = std::sqrt(disc);
  const float t_far = -b + sqrt_disc;
  if (t_far < 0.0f) {
    return false;
  }
  out_distance = rayClosestDistance(ray, center);
  return true;
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
  const float plane_slop =
      pickWorldSlop(arrow_scale, ctx.gizmo_pixel_size, TransformGizmoMetrics::k_min_pick_pixels);
  const float plane_half = plane_scale * TransformGizmoMetrics::k_mesh_plane_half_extent;
  const float center_r = center_scale * TransformGizmoMetrics::k_mesh_center_radius;
  const float axis_inner = center_r;
  const float cone_z = arrow_scale * TransformGizmoMetrics::k_mesh_cone_base_z;
  const float cone_r = arrow_scale * TransformGizmoMetrics::k_mesh_cone_base_radius;

  eastl::vector<eastl::pair<ManipulatorAxis, float>> hits;
  hits.reserve(10);

  auto try_axis = [&](ManipulatorAxis axis, const glm::vec3& dir) {
    if (!handlePickable(ctx, axis)) {
      return;
    }
    const glm::vec3 ndir = glm::normalize(dir);
    const glm::vec3 start = ctx.basis.origin + ndir * axis_inner;
    const glm::vec3 tip = ctx.basis.origin + ndir * axis_len;
    const float slop = axisPickSlop(ctx, arrow_scale, start, tip);
    float dist = 0.0f;
    if (pickAxis(ctx.ray, start, dir, axis_len - axis_inner, slop, dist)) {
      hits.push_back({axis, dist});
    }
    const glm::vec3 cone = ctx.basis.origin + ndir * cone_z;
    if (pickCenter(ctx.ray, cone, cone_r, slop, dist) ||
        pickCenter(ctx.ray, tip, cone_r, slop, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_axis(ManipulatorAxis::trans_x, ctx.basis.axis_x);
  try_axis(ManipulatorAxis::trans_y, ctx.basis.axis_y);
  try_axis(ManipulatorAxis::trans_z, ctx.basis.axis_z);

  const float plane_offset =
      plane_scale * TransformGizmoMetrics::k_mesh_plane_center_offset;
  auto try_plane = [&](ManipulatorAxis axis, const glm::vec3& u, const glm::vec3& v) {
    if (!handlePickable(ctx, axis)) {
      return;
    }
    const glm::vec3 center = ctx.basis.origin + glm::normalize(u) * plane_offset +
                             glm::normalize(v) * plane_offset;
    float dist = 0.0f;
    if (pickPlaneHandle(ctx.ray, center, u, v, plane_half, plane_slop, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_plane(ManipulatorAxis::trans_xy, ctx.basis.axis_x, ctx.basis.axis_y);
  try_plane(ManipulatorAxis::trans_yz, ctx.basis.axis_y, ctx.basis.axis_z);
  try_plane(ManipulatorAxis::trans_zx, ctx.basis.axis_z, ctx.basis.axis_x);

  {
    float dist = 0.0f;
    if (pickCenter(ctx.ray, ctx.basis.origin, center_r, plane_slop, dist)) {
      hits.push_back({ManipulatorAxis::trans_c, dist});
    }
  }

  if (hits.empty()) {
    return std::nullopt;
  }
  return pickBest(hits);
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
  const float plane_slop =
      pickWorldSlop(axis_scale, ctx.gizmo_pixel_size,
                    TransformGizmoMetrics::k_min_pick_pixels);
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
    if (!handlePickable(ctx, axis)) {
      return;
    }
    const glm::vec3 start = ctx.basis.origin + glm::normalize(dir) * stem_start;
    const glm::vec3 end = ctx.basis.origin + glm::normalize(dir) * stem_end;
    const float slop = axisPickSlop(ctx, axis_scale, start, end);
    float dist = 0.0f;
    if (pickAxis(ctx.ray, start, end - start, glm::length(end - start), slop, dist)) {
      hits.push_back({axis, dist});
    }
  };

  auto try_box = [&](const ManipulatorAxis axis, const glm::vec3& center,
                     const float half_extent, const float slop) {
    if (!handlePickable(ctx, axis)) {
      return;
    }
    float dist = 0.0f;
    if (pickCenter(ctx.ray, center, half_extent, slop, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_stem(ManipulatorAxis::trans_x, ctx.basis.axis_x);
  try_stem(ManipulatorAxis::trans_y, ctx.basis.axis_y);
  try_stem(ManipulatorAxis::trans_z, ctx.basis.axis_z);

  const float box_slop =
      pickWorldSlop(axis_scale, ctx.gizmo_pixel_size,
                    TransformGizmoMetrics::k_axis_pick_pixels);
  try_box(ManipulatorAxis::trans_x,
          scaleHandleWorldCenter(ctx.basis, ManipulatorAxis::trans_x, axis_scale),
          axis_half, box_slop);
  try_box(ManipulatorAxis::trans_y,
          scaleHandleWorldCenter(ctx.basis, ManipulatorAxis::trans_y, axis_scale),
          axis_half, box_slop);
  try_box(ManipulatorAxis::trans_z,
          scaleHandleWorldCenter(ctx.basis, ManipulatorAxis::trans_z, axis_scale),
          axis_half, box_slop);

  auto try_plane = [&](const ManipulatorAxis axis, const glm::vec3& u, const glm::vec3& v) {
    if (!handlePickable(ctx, axis)) {
      return;
    }
    const glm::vec3 center = ctx.basis.origin + glm::normalize(u) * plane_offset +
                             glm::normalize(v) * plane_offset;
    float dist = 0.0f;
    if (pickPlaneHandle(ctx.ray, center, u, v, plane_half, plane_slop, dist)) {
      hits.push_back({axis, dist});
    }
  };

  try_plane(ManipulatorAxis::trans_xy, ctx.basis.axis_x, ctx.basis.axis_y);
  try_plane(ManipulatorAxis::trans_yz, ctx.basis.axis_y, ctx.basis.axis_z);
  try_plane(ManipulatorAxis::trans_zx, ctx.basis.axis_z, ctx.basis.axis_x);

  try_box(ManipulatorAxis::trans_c, ctx.basis.origin, center_half, plane_slop);

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
