#include "runtime/function/render/gizmo/transform_gizmo_pick.h"

#include <cmath>

#include "EASTL/vector.h"

namespace Blunder {

namespace {

float pickThreshold(const float uniform_scale) {
  return uniform_scale * TransformGizmoMetrics::k_axis_radius_factor *
         TransformGizmoMetrics::k_pick_slop * 40.0f;
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
  const auto hit =
      intersectRayPlaneQuad(ray, center, axis_u, axis_v, half_extent);
  if (!hit) {
    return false;
  }
  const float dist = glm::length(*hit - ray.origin);
  out_distance = dist;
  return dist < half_extent * 4.0f + threshold * 8.0f;
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
  const float threshold = tube_radius * TransformGizmoMetrics::k_pick_slop * 2.5f;
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
  const float scale = ctx.uniform_scale;
  const float axis_len = scale;
  const float threshold = pickThreshold(scale);
  const float plane_half = scale * TransformGizmoMetrics::k_plane_half_extent_factor /
                           TransformGizmoMetrics::k_axis_length_factor;
  const float center_r =
      scale * TransformGizmoMetrics::k_center_radius_factor /
      TransformGizmoMetrics::k_axis_length_factor;

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

  auto try_plane = [&](ManipulatorAxis axis, const glm::vec3& u, const glm::vec3& v) {
    float dist = 0.0f;
    if (pickPlaneHandle(ctx.ray, ctx.basis.origin, u, v, plane_half, threshold, dist)) {
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
  return pickBest(hits);
}

std::optional<ManipulatorAxis> pickRotationGizmoHandle(
    const TransformGizmoPickContext& ctx) {
  const float scale = ctx.uniform_scale;
  const float major_r = scale * TransformGizmoMetrics::k_dial_major_radius_factor;
  const float tube_r = scale * TransformGizmoMetrics::k_dial_tube_radius_factor;

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

}  // namespace Blunder
