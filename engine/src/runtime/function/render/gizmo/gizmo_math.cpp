#include "runtime/function/render/gizmo/gizmo_math.h"

#include <algorithm>
#include <cmath>

#include "runtime/core/base/macro.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

namespace {

glm::vec3 pickPerpendicularReference(const glm::vec3& local_z, const GizmoBasis& basis) {
  const float dx = std::abs(glm::dot(local_z, basis.axis_x));
  const float dy = std::abs(glm::dot(local_z, basis.axis_y));
  const float dz = std::abs(glm::dot(local_z, basis.axis_z));
  if (dy <= dx && dy <= dz) {
    return basis.axis_y;
  }
  if (dz <= dx && dz <= dy) {
    return basis.axis_z;
  }
  return basis.axis_x;
}

glm::mat4 buildOrthonormalHandleMatrix(const GizmoBasis& basis, const glm::vec3& local_z,
                                       const float uniform_scale) {
  const glm::vec3 z = glm::normalize(local_z);
  const glm::vec3 ref = pickPerpendicularReference(z, basis);
  glm::vec3 x = glm::cross(ref, z);
  if (glm::length(x) < 1e-4f) {
    x = glm::cross(z, basis.axis_z);
  }
  if (glm::length(x) < 1e-4f) {
    x = glm::cross(z, basis.axis_y);
  }
  x = glm::normalize(x);
  const glm::vec3 y = glm::normalize(glm::cross(z, x));

  glm::mat4 m(1.0f);
  m[0] = glm::vec4(x * uniform_scale, 0.0f);
  m[1] = glm::vec4(y * uniform_scale, 0.0f);
  m[2] = glm::vec4(z * uniform_scale, 0.0f);
  m[3] = glm::vec4(basis.origin, 1.0f);
  return m;
}

}  // namespace

GizmoBasis buildGizmoBasis(const glm::mat4& selection_world, const GizmoSpace space) {
  GizmoBasis basis;
  basis.origin = glm::vec3(selection_world[3]);
  basis.axis_x = glm::normalize(glm::vec3(selection_world[0]));
  basis.axis_y = glm::normalize(glm::vec3(selection_world[1]));
  basis.axis_z = glm::normalize(glm::vec3(selection_world[2]));

  glm::mat3 rot(1.0f);
  if (space == GizmoSpace::global) {
    rot = glm::mat3(1.0f);
    basis.axis_x = glm::vec3(1.0f, 0.0f, 0.0f);
    basis.axis_y = glm::vec3(0.0f, 1.0f, 0.0f);
    basis.axis_z = glm::vec3(0.0f, 0.0f, 1.0f);
  } else {
    rot = glm::mat3(selection_world);
  }

  basis.matrix = glm::mat4(rot);
  basis.matrix[3] = glm::vec4(basis.origin, 1.0f);
  return basis;
}

glm::quat worldRotationFromMatrix(const glm::mat4& world_matrix) {
  const glm::mat3 rot(glm::normalize(glm::vec3(world_matrix[0])),
                      glm::normalize(glm::vec3(world_matrix[1])),
                      glm::normalize(glm::vec3(world_matrix[2])));
  return glm::normalize(glm::quat_cast(rot));
}

namespace {

float perspectivePixsizeFromMatrixColumns(const glm::mat4& matrix, const float viewfac) {
  const glm::vec3 col0 = glm::vec3(matrix[0]);
  const glm::vec3 col1 = glm::vec3(matrix[1]);
  const float len0 = glm::dot(col0, col0);
  const float len1 = glm::dot(col1, col1);
  const float len_px = 2.0f / std::sqrt(std::max(std::min(len0, len1), 1e-8f));
  return len_px / viewfac;
}

}  // namespace

float computeViewportPixsize(const glm::mat4& projection, const float viewport_width,
                             const float viewport_height, const bool is_perspective,
                             const float ortho_size) {
  const float viewfac =
      std::max(std::max(viewport_width, viewport_height), 1.0f);
  if (!is_perspective) {
    return ortho_size / viewfac;
  }

  return perspectivePixsizeFromMatrixColumns(projection, viewfac);
}

float computeGizmoPixelSizeNoUiScale(const TransformGizmoScaleContext& ctx) {
  const float pixsize =
      computeViewportPixsize(ctx.projection, ctx.viewport_width, ctx.viewport_height,
                             ctx.is_perspective, ctx.ortho_size);
  if (!ctx.is_perspective) {
    return pixsize;
  }
  const glm::vec4 clip = ctx.view_projection * glm::vec4(ctx.pivot, 1.0f);
  const float depth = std::max(std::abs(clip.w), 1e-4f);
  return pixsize * depth;
}

float computeView3dPixelSizeNoUiScale(const TransformGizmoScaleContext& ctx) {
  return computeGizmoPixelSizeNoUiScale(ctx);
}

namespace {

float computeEffectiveGizmoSize(const TransformGizmoScaleContext& ctx) {
  const float vp_min = std::min(ctx.viewport_width, ctx.viewport_height);
  const float cap = vp_min * TransformGizmoMetrics::k_viewport_gizmo_size_factor;
  return std::min(ctx.gizmo_size, cap);
}

}  // namespace

float computeGizmoGroupScale(const TransformGizmoScaleContext& ctx) {
  const float pixel_size = computeGizmoPixelSizeNoUiScale(ctx);
  return ctx.ui_scale * computeEffectiveGizmoSize(ctx) * pixel_size;
}

#ifndef NDEBUG
void debugLogGizmoScaleParity(const TransformGizmoScaleContext& perspective,
                              const TransformGizmoScaleContext& orthographic) {
  const float persp_scale = computeGizmoGroupScale(perspective);
  const float ortho_scale = computeGizmoGroupScale(orthographic);
  if (ortho_scale <= 1e-6f) {
    return;
  }
  const float ratio = persp_scale / ortho_scale;
  if (ratio < 0.85f || ratio > 1.15f) {
    LOG_WARN("[TransformGizmo] Persp/Ortho group_scale ratio {:.2f} outside [0.85, 1.15]",
             ratio);
  }
}
#endif

float computeGizmoHandleScale(const float group_scale, const ManipulatorAxis axis) {
  return group_scale * gizmoScaleBasisForAxis(axis);
}

float computeGizmoHandleScale(const TransformGizmoScaleContext& ctx,
                              const ManipulatorAxis axis) {
  return computeGizmoHandleScale(computeGizmoGroupScale(ctx), axis);
}

uint32_t computeDialSides(const float screen_radius_px) {
  using M = TransformGizmoMetrics;
  if (screen_radius_px < 1.0f) {
    return M::k_dial_sides_min;
  }
  const float circumference = 2.0f * 3.14159265f * screen_radius_px;
  const uint32_t sides = static_cast<uint32_t>(
      std::ceil(circumference / M::k_dial_target_chord_px));
  return std::clamp(sides, M::k_dial_sides_min, M::k_dial_sides_max);
}

float computeRingScreenRadiusPx(const glm::mat4& view, const glm::mat4& proj,
                                  const glm::mat4& gizmo_world,
                                  const float viewport_width,
                                  const float viewport_height,
                                  const float mesh_radius) {
  const glm::vec3 center = glm::vec3(gizmo_world[3]);
  const glm::vec3 world_x = glm::vec3(gizmo_world[0]) * mesh_radius;
  const glm::vec3 world_y = glm::vec3(gizmo_world[1]) * mesh_radius;

  const auto to_screen = [&](const glm::vec3& world) -> glm::vec2 {
    const glm::vec4 clip = proj * view * glm::vec4(world, 1.0f);
    const float w = std::max(std::abs(clip.w), 1e-6f);
    const glm::vec2 ndc = glm::vec2(clip) / w;
    return glm::vec2((ndc.x * 0.5f + 0.5f) * viewport_width,
                     (0.5f - ndc.y * 0.5f) * viewport_height);
  };

  const glm::vec2 center_px = to_screen(center);
  const float radius_x = glm::length(to_screen(center + world_x) - center_px);
  const float radius_y = glm::length(to_screen(center + world_y) - center_px);
  return std::max(radius_x, radius_y);
}

void computeGizmoIdot(const GizmoBasis& basis, const glm::vec3& camera_position,
                      const glm::vec3& pivot, float out_idot[3]) {
  const glm::vec3 view_dir = glm::normalize(camera_position - pivot);
  const glm::vec3 axes[3] = {glm::normalize(basis.axis_x), glm::normalize(basis.axis_y),
                               glm::normalize(basis.axis_z)};
  for (int i = 0; i < 3; ++i) {
    out_idot[i] = 1.0f - std::abs(glm::dot(view_dir, axes[i]));
  }
}

glm::mat4 gizmoViewAlignedCenterMatrix(const GizmoBasis& basis, const float uniform_scale,
                                       const glm::vec3& camera_position) {
  glm::vec3 local_z = glm::normalize(camera_position - basis.origin);
  glm::vec3 local_x = glm::normalize(glm::cross(basis.axis_y, local_z));
  if (glm::length(local_x) < 1e-4f) {
    local_x = glm::normalize(glm::cross(basis.axis_z, local_z));
  }
  const glm::vec3 local_y = glm::normalize(glm::cross(local_z, local_x));

  glm::mat4 m(1.0f);
  m[0] = glm::vec4(local_x * uniform_scale, 0.0f);
  m[1] = glm::vec4(local_y * uniform_scale, 0.0f);
  m[2] = glm::vec4(local_z * uniform_scale, 0.0f);
  m[3] = glm::vec4(basis.origin, 1.0f);
  return m;
}

glm::mat4 gizmoHandleMatrix(const GizmoBasis& basis, const ManipulatorAxis axis,
                            const float uniform_scale) {
  glm::vec3 local_z = glm::vec3(0.0f, 0.0f, 1.0f);
  glm::vec3 local_x = glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 local_y = glm::vec3(0.0f, 1.0f, 0.0f);

  switch (axis) {
    case ManipulatorAxis::trans_x:
      local_z = basis.axis_x;
      break;
    case ManipulatorAxis::trans_y:
      local_z = basis.axis_y;
      break;
    case ManipulatorAxis::trans_z:
      local_z = basis.axis_z;
      break;
    case ManipulatorAxis::trans_xy:
      local_z = basis.axis_z;
      local_x = basis.axis_x;
      local_y = basis.axis_y;
      break;
    case ManipulatorAxis::trans_yz:
      local_z = basis.axis_x;
      local_x = basis.axis_y;
      local_y = basis.axis_z;
      break;
    case ManipulatorAxis::trans_zx:
      local_z = basis.axis_y;
      local_x = basis.axis_z;
      local_y = basis.axis_x;
      break;
    case ManipulatorAxis::trans_c:
      local_z = basis.axis_z;
      local_x = basis.axis_x;
      local_y = basis.axis_y;
      break;
    default:
      break;
  }

  if (axis == ManipulatorAxis::trans_x || axis == ManipulatorAxis::trans_y ||
      axis == ManipulatorAxis::trans_z) {
    return buildOrthonormalHandleMatrix(basis, local_z, uniform_scale);
  }

  glm::mat4 m(1.0f);
  m[0] = glm::vec4(glm::normalize(local_x) * uniform_scale, 0.0f);
  m[1] = glm::vec4(glm::normalize(local_y) * uniform_scale, 0.0f);
  m[2] = glm::vec4(glm::normalize(local_z) * uniform_scale, 0.0f);
  m[3] = glm::vec4(basis.origin, 1.0f);
  return m;
}

glm::mat4 gizmoScaleBoxMatrix(const GizmoBasis& basis, const ManipulatorAxis axis,
                              const float uniform_scale) {
  glm::vec3 local_z = basis.axis_z;
  switch (axis) {
    case ManipulatorAxis::trans_x:
      local_z = basis.axis_x;
      break;
    case ManipulatorAxis::trans_y:
      local_z = basis.axis_y;
      break;
    case ManipulatorAxis::trans_z:
      local_z = basis.axis_z;
      break;
    case ManipulatorAxis::trans_c:
      return gizmoHandleMatrix(basis, ManipulatorAxis::trans_c, uniform_scale);
    default:
      break;
  }
  return buildOrthonormalHandleMatrix(basis, local_z, uniform_scale);
}

glm::vec3 scaleHandleWorldCenter(const GizmoBasis& basis, const ManipulatorAxis axis,
                                 const float handle_scale) {
  const float offset =
      handle_scale * TransformGizmoMetrics::k_mesh_scale_box_center_offset;
  switch (axis) {
    case ManipulatorAxis::trans_x:
      return basis.origin + basis.axis_x * offset;
    case ManipulatorAxis::trans_y:
      return basis.origin + basis.axis_y * offset;
    case ManipulatorAxis::trans_z:
      return basis.origin + basis.axis_z * offset;
  }
  return basis.origin;
}

Vec3 applyScaleDrag(const Vec3& scale_at_drag_start, const ManipulatorAxis axis,
                    const float factor) {
  const float clamped_factor = std::max(factor, 0.01f);
  Vec3 result = scale_at_drag_start;
  if (axis == ManipulatorAxis::trans_c) {
    return result * clamped_factor;
  }
  bool mask[3]{};
  manipulatorTranslationMask(axis, mask);
  if (mask[0]) {
    result.x *= clamped_factor;
  }
  if (mask[1]) {
    result.y *= clamped_factor;
  }
  if (mask[2]) {
    result.z *= clamped_factor;
  }
  return result;
}

glm::vec3 closestPointOnLine(const glm::vec3& origin, const glm::vec3& direction,
                             const glm::vec3& point) {
  const glm::vec3 dir = glm::normalize(direction);
  const float t = glm::dot(point - origin, dir);
  return origin + dir * t;
}

std::optional<float> rayLineParameter(const Ray& ray, const glm::vec3& line_origin,
                                      const glm::vec3& line_direction) {
  const glm::vec3 d = glm::normalize(line_direction);
  const glm::vec3 w0 = ray.origin - line_origin;
  const float a = glm::dot(ray.direction, ray.direction);
  const float b = glm::dot(ray.direction, d);
  const float c = glm::dot(d, d);
  const float d_coef = glm::dot(ray.direction, w0);
  const float e = glm::dot(d, w0);
  const float denom = a * c - b * b;
  if (std::abs(denom) < 1e-8f) {
    return std::nullopt;
  }
  return (b * e - c * d_coef) / denom;
}

std::optional<glm::vec3> intersectRayPlane(const Ray& ray, const Plane& plane) {
  const auto hit = intersect(ray, plane);
  if (!hit) {
    return std::nullopt;
  }
  return hit->point;
}

std::optional<glm::vec3> intersectRayPlaneQuad(const Ray& ray,
                                               const glm::vec3& center,
                                               const glm::vec3& axis_u,
                                               const glm::vec3& axis_v,
                                               const float half_extent) {
  const glm::vec3 normal = glm::normalize(glm::cross(axis_u, axis_v));
  const Plane plane = Plane::fromPointAndNormal(center, normal);
  const auto hit = intersectRayPlane(ray, plane);
  if (!hit) {
    return std::nullopt;
  }
  const glm::vec3 local = *hit - center;
  const float u = glm::dot(local, glm::normalize(axis_u));
  const float v = glm::dot(local, glm::normalize(axis_v));
  if (std::abs(u) > half_extent || std::abs(v) > half_extent) {
    return std::nullopt;
  }
  return hit;
}

float distanceRayToSegment(const Ray& ray, const glm::vec3& a, const glm::vec3& b) {
  const glm::vec3 ab = b - a;
  const float ab_len_sq = glm::dot(ab, ab);
  if (ab_len_sq < 1e-8f) {
    return glm::length(glm::cross(ray.direction, a - ray.origin)) /
           std::max(glm::length(ray.direction), 1e-6f);
  }
  const float t = std::clamp(glm::dot(ray.origin - a, ab) / ab_len_sq, 0.0f, 1.0f);
  const glm::vec3 closest = a + ab * t;
  return glm::length(glm::cross(ray.direction, closest - ray.origin)) /
         std::max(glm::length(ray.direction), 1e-6f);
}

glm::vec3 worldTranslationDelta(const GizmoBasis& basis, const ManipulatorAxis axis,
                                const glm::vec3& drag_start_world,
                                const glm::vec3& current_world) {
  const glm::vec3 delta = current_world - drag_start_world;
  if (axis == ManipulatorAxis::trans_c) {
    return delta;
  }
  if (axis == ManipulatorAxis::trans_x) {
    return basis.axis_x * glm::dot(delta, basis.axis_x);
  }
  if (axis == ManipulatorAxis::trans_y) {
    return basis.axis_y * glm::dot(delta, basis.axis_y);
  }
  if (axis == ManipulatorAxis::trans_z) {
    return basis.axis_z * glm::dot(delta, basis.axis_z);
  }
  if (axis == ManipulatorAxis::trans_xy) {
    return basis.axis_x * glm::dot(delta, basis.axis_x) +
           basis.axis_y * glm::dot(delta, basis.axis_y);
  }
  if (axis == ManipulatorAxis::trans_yz) {
    return basis.axis_y * glm::dot(delta, basis.axis_y) +
           basis.axis_z * glm::dot(delta, basis.axis_z);
  }
  if (axis == ManipulatorAxis::trans_zx) {
    return basis.axis_z * glm::dot(delta, basis.axis_z) +
           basis.axis_x * glm::dot(delta, basis.axis_x);
  }
  return delta;
}

glm::mat4 gizmoDialMatrix(const GizmoBasis& basis, const ManipulatorAxis axis,
                          const float uniform_scale) {
  glm::vec3 local_z = basis.axis_z;
  glm::vec3 local_x = basis.axis_x;
  glm::vec3 local_y = basis.axis_y;

  switch (axis) {
    case ManipulatorAxis::rot_x:
      local_z = basis.axis_x;
      local_x = basis.axis_y;
      local_y = basis.axis_z;
      break;
    case ManipulatorAxis::rot_y:
      local_z = basis.axis_y;
      local_x = basis.axis_z;
      local_y = basis.axis_x;
      break;
    case ManipulatorAxis::rot_z:
      local_z = basis.axis_z;
      local_x = basis.axis_x;
      local_y = basis.axis_y;
      break;
    default:
      break;
  }

  glm::mat4 m(1.0f);
  m[0] = glm::vec4(glm::normalize(local_x) * uniform_scale, 0.0f);
  m[1] = glm::vec4(glm::normalize(local_y) * uniform_scale, 0.0f);
  m[2] = glm::vec4(glm::normalize(local_z) * uniform_scale, 0.0f);
  m[3] = glm::vec4(basis.origin, 1.0f);
  return m;
}

std::optional<glm::vec3> intersectRayWithAxisPlane(const Ray& ray,
                                                    const glm::vec3& pivot,
                                                    const glm::vec3& axis) {
  const Plane plane = Plane::fromPointAndNormal(pivot, glm::normalize(axis));
  return intersectRayPlane(ray, plane);
}

float signedAngleOnRotationPlane(const glm::vec3& axis, const glm::vec3& pivot,
                                 const glm::vec3& reference,
                                 const glm::vec3& point) {
  const glm::vec3 n = glm::normalize(axis);
  auto project = [&](const glm::vec3& v) {
    glm::vec3 r = v - pivot;
    r -= n * glm::dot(r, n);
    const float len = glm::length(r);
    return len > 1e-6f ? r / len : glm::vec3(0.0f);
  };

  const glm::vec3 ref_dir = project(reference);
  const glm::vec3 point_dir = project(point);
  if (glm::length(ref_dir) < 1e-5f || glm::length(point_dir) < 1e-5f) {
    return 0.0f;
  }

  const glm::vec3 tangent = glm::cross(n, ref_dir);
  const float x = glm::dot(point_dir, ref_dir);
  const float y = glm::dot(point_dir, tangent);
  return std::atan2(y, x);
}

float rotationDialMeshAngle(const GizmoBasis& basis, const ManipulatorAxis axis,
                            const glm::vec3& point_on_plane) {
  glm::vec3 local_x = basis.axis_x;
  glm::vec3 local_y = basis.axis_y;
  glm::vec3 normal = basis.axis_z;

  switch (axis) {
    case ManipulatorAxis::rot_x:
      normal = basis.axis_x;
      local_x = basis.axis_y;
      local_y = basis.axis_z;
      break;
    case ManipulatorAxis::rot_y:
      normal = basis.axis_y;
      local_x = basis.axis_z;
      local_y = basis.axis_x;
      break;
    case ManipulatorAxis::rot_z:
      normal = basis.axis_z;
      local_x = basis.axis_x;
      local_y = basis.axis_y;
      break;
    default:
      break;
  }

  glm::vec3 radial = point_on_plane - basis.origin;
  radial -= normal * glm::dot(radial, normal);
  const float x = glm::dot(radial, glm::normalize(local_x));
  const float y = glm::dot(radial, glm::normalize(local_y));
  float angle = std::atan2(y, x);
  if (angle < 0.0f) {
    angle += glm::two_pi<float>();
  }
  return angle;
}

float wrapAngle(const float radians) {
  float a = std::fmod(radians + glm::pi<float>(), glm::two_pi<float>());
  if (a < 0.0f) {
    a += glm::two_pi<float>();
  }
  return a - glm::pi<float>();
}

Quat applyRotationDrag(const Quat& rotation_at_drag_start, const ManipulatorAxis axis,
                       const glm::vec3& world_axis, const float angle_delta,
                       const GizmoSpace space, const Quat& parent_world_rotation) {
  if (space == GizmoSpace::local) {
    glm::vec3 local_axis(1.0f, 0.0f, 0.0f);
    if (axis == ManipulatorAxis::rot_y) {
      local_axis = glm::vec3(0.0f, 1.0f, 0.0f);
    } else if (axis == ManipulatorAxis::rot_z) {
      local_axis = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    const Quat local_delta = glm::angleAxis(angle_delta, local_axis);
    return glm::normalize(rotation_at_drag_start * local_delta);
  }

  const glm::vec3 world_axis_norm = glm::normalize(world_axis);
  const Quat delta = glm::angleAxis(angle_delta, world_axis_norm);
  const Quat world_start =
      glm::normalize(parent_world_rotation * rotation_at_drag_start);
  const Quat world_new = glm::normalize(delta * world_start);
  return glm::normalize(glm::inverse(parent_world_rotation) * world_new);
}

Quat parentWorldRotation(SceneInstance& scene, const EntityId entity_id) {
  const Entity* entity = scene.getEntity(entity_id);
  if (entity == nullptr || !isValid(entity->getParentId())) {
    return glm::identity<Quat>();
  }
  const glm::mat3 parent_rot =
      glm::mat3(scene.getWorldMatrix(entity->getParentId()));
  return glm::normalize(glm::quat_cast(parent_rot));
}

glm::vec4 buildDialClipPlane(const glm::vec3& pivot, const glm::vec3& camera_position,
                             const float pixel_size) {
  glm::vec3 n = glm::normalize(camera_position - pivot);
  float d = -glm::dot(n, pivot);
  d += TransformGizmoMetrics::k_dial_clip_bias * pixel_size;
  return glm::vec4(n, d);
}

}  // namespace Blunder
