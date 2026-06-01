#include "runtime/function/render/gizmo/gizmo_math.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

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

float computeHandleUniformScale(const glm::vec3& camera_position,
                                const glm::vec3& pivot, const float viewport_height,
                                const float vertical_fov, const bool is_perspective,
                                const float ortho_size) {
  const float dist = std::max(glm::length(camera_position - pivot), 0.01f);
  if (is_perspective) {
  const float tan_half = std::tan(vertical_fov * 0.5f);
    return dist * tan_half * TransformGizmoMetrics::k_axis_length_factor * 8.0f;
  }
  const float vp = std::max(viewport_height, 1.0f);
  return (ortho_size / vp) * 120.0f * TransformGizmoMetrics::k_axis_length_factor;
}

float viewAlignedAxisAlpha(const glm::vec3& axis_dir_world,
                           const glm::vec3& camera_forward,
                           const glm::vec3& camera_position,
                           const glm::vec3& pivot) {
  const glm::vec3 view_dir = glm::normalize(camera_position - pivot);
  const float axis_dot = std::abs(glm::dot(axis_dir_world, view_dir));
  const float fade =
      (axis_dot - TransformGizmoMetrics::k_view_fade_dot_threshold) /
      std::max(1.0f - TransformGizmoMetrics::k_view_fade_dot_threshold, 1e-4f);
  const float alpha = 1.0f - std::clamp(fade, 0.0f, 1.0f);
  return std::max(alpha, TransformGizmoMetrics::k_min_view_alpha);
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
    local_x = glm::normalize(glm::cross(local_z, basis.axis_z));
    if (glm::length(local_x) < 1e-4f) {
      local_x = glm::normalize(glm::cross(local_z, basis.axis_y));
    }
    local_y = glm::normalize(glm::cross(local_z, local_x));
  }

  glm::mat4 m(1.0f);
  m[0] = glm::vec4(glm::normalize(local_x) * uniform_scale, 0.0f);
  m[1] = glm::vec4(glm::normalize(local_y) * uniform_scale, 0.0f);
  m[2] = glm::vec4(glm::normalize(local_z) * uniform_scale, 0.0f);
  m[3] = glm::vec4(basis.origin, 1.0f);
  return m;
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

}  // namespace Blunder
