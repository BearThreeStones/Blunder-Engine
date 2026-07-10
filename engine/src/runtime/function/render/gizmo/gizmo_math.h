#pragma once

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;

struct GizmoBasis {
  glm::mat4 matrix{1.0f};
  glm::vec3 origin{0.0f};
  glm::vec3 axis_x{1.0f, 0.0f, 0.0f};
  glm::vec3 axis_y{0.0f, 1.0f, 0.0f};
  glm::vec3 axis_z{0.0f, 0.0f, 1.0f};
};

GizmoBasis buildGizmoBasis(const glm::mat4& selection_world, GizmoSpace space);

/// World-space rotation from a TRS/world matrix (scale stripped from basis columns).
glm::quat worldRotationFromMatrix(const glm::mat4& world_matrix);

/// Inputs for Blender wm_gizmo_calculate_scale / ED_view3d_pixel_size_no_ui_scale.
struct TransformGizmoScaleContext {
  glm::mat4 view_projection{1.0f};
  glm::vec3 pivot{0.0f};
  float viewport_height{1.0f};
  bool is_perspective{true};
  float ortho_size{10.0f};
  float gizmo_size{TransformGizmoMetrics::k_default_gizmo_size};
  float ui_scale{TransformGizmoMetrics::k_ui_scale_factor};
};

/// Blender ED_view3d_pixel_size_no_ui_scale: zfac(persmat, co) * pixsize.
float computeView3dPixelSizeNoUiScale(const TransformGizmoScaleContext& ctx);

/// Blender wm_gizmo_calculate_scale group factor (before per-handle scale_basis).
float computeGizmoGroupScale(const TransformGizmoScaleContext& ctx);

/// scale_final = scale_basis * group_scale.
float computeGizmoHandleScale(const TransformGizmoScaleContext& ctx,
                              ManipulatorAxis axis);

float computeGizmoHandleScale(float group_scale, ManipulatorAxis axis);

/// Blender gizmo_get_idot: per-axis view alignment metric.
void computeGizmoIdot(const GizmoBasis& basis, const glm::vec3& camera_position,
                      const glm::vec3& pivot, float out_idot[3]);

glm::mat4 gizmoHandleMatrix(const GizmoBasis& basis, ManipulatorAxis axis,
                            float uniform_scale);

glm::mat4 gizmoScaleBoxMatrix(const GizmoBasis& basis, ManipulatorAxis axis,
                              float uniform_scale);

glm::vec3 scaleHandleWorldCenter(const GizmoBasis& basis, ManipulatorAxis axis,
                                 float handle_scale);

Vec3 applyScaleDrag(const Vec3& scale_at_drag_start, ManipulatorAxis axis, float factor);

/// Blender draw_prepare view-aligned center (TH_GIZMO_VIEW_ALIGN circle).
glm::mat4 gizmoViewAlignedCenterMatrix(const GizmoBasis& basis, float uniform_scale,
                                       const glm::vec3& camera_position);

/// Closest point on infinite line (origin + t * direction).
glm::vec3 closestPointOnLine(const glm::vec3& origin, const glm::vec3& direction,
                             const glm::vec3& point);

/// Ray-line closest approach; returns line parameter t.
std::optional<float> rayLineParameter(const Ray& ray, const glm::vec3& line_origin,
                                      const glm::vec3& line_direction);

/// Intersect ray with plane; optional clamp to finite quad in plane.
std::optional<glm::vec3> intersectRayPlane(const Ray& ray, const Plane& plane);

std::optional<glm::vec3> intersectRayPlaneQuad(const Ray& ray, const glm::vec3& center,
                                               const glm::vec3& axis_u,
                                               const glm::vec3& axis_v, float half_extent);

float distanceRayToSegment(const Ray& ray, const glm::vec3& a, const glm::vec3& b);

glm::vec3 worldTranslationDelta(const GizmoBasis& basis, ManipulatorAxis axis,
                                const glm::vec3& drag_start_world,
                                const glm::vec3& current_world);

glm::mat4 gizmoDialMatrix(const GizmoBasis& basis, ManipulatorAxis axis,
                          float uniform_scale);

std::optional<glm::vec3> intersectRayWithAxisPlane(const Ray& ray,
                                                    const glm::vec3& pivot,
                                                    const glm::vec3& axis);

float signedAngleOnRotationPlane(const glm::vec3& axis, const glm::vec3& pivot,
                                 const glm::vec3& reference, const glm::vec3& point);

/// Angle in dial mesh space [0, 2π) for a point on the rotation plane.
float rotationDialMeshAngle(const GizmoBasis& basis, ManipulatorAxis axis,
                            const glm::vec3& point_on_plane);

float wrapAngle(float radians);

/// Applies rotation drag delta to entity local quaternion (handles global/local + parent).
Quat applyRotationDrag(const Quat& rotation_at_drag_start, ManipulatorAxis axis,
                       const glm::vec3& world_axis, float angle_delta, GizmoSpace space,
                       const Quat& parent_world_rotation);

Quat parentWorldRotation(SceneInstance& scene, EntityId entity_id);

}  // namespace Blunder
