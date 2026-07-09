#include "runtime/function/render/gizmo/translate_modal_session.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "runtime/function/render/editor_camera.h"

namespace Blunder {

namespace {

glm::vec3 normalizedOr(const glm::vec3& value, const glm::vec3& fallback) {
  const float len2 = glm::dot(value, value);
  if (len2 <= 1e-8f) {
    return fallback;
  }
  return value / std::sqrt(len2);
}

glm::vec3 axisDirectionFor(const ManipulatorAxis axis, const GizmoBasis& basis) {
  if (axis == ManipulatorAxis::trans_y) {
    return basis.axis_y;
  }
  if (axis == ManipulatorAxis::trans_z) {
    return basis.axis_z;
  }
  return basis.axis_x;
}

glm::vec3 projectOntoPlaneAxes(const glm::vec3& delta, const glm::vec3& axis_a,
                               const glm::vec3& axis_b) {
  const glm::vec3 a = normalizedOr(axis_a, glm::vec3(1.0f, 0.0f, 0.0f));
  const glm::vec3 b = normalizedOr(axis_b, glm::vec3(0.0f, 1.0f, 0.0f));
  return a * glm::dot(delta, a) + b * glm::dot(delta, b);
}

}  // namespace

glm::vec3 screenDeltaToViewPlaneWorld(
    const glm::vec3& camera_position, const glm::vec3& camera_forward,
    const glm::vec3& camera_right, const glm::vec3& camera_up,
    const float viewport_height_world_per_pixel,
    const glm::vec2& pixel_delta) {
  (void)camera_position;
  (void)camera_forward;
  const glm::vec3 right = normalizedOr(camera_right, glm::vec3(1.0f, 0.0f, 0.0f));
  const glm::vec3 up = normalizedOr(camera_up, glm::vec3(0.0f, 1.0f, 0.0f));
  return (right * pixel_delta.x - up * pixel_delta.y) *
         viewport_height_world_per_pixel;
}

glm::vec3 constrainTranslationDelta(const glm::vec3& free_delta,
                                    const ManipulatorAxis axis,
                                    const GizmoBasis& basis,
                                    const glm::vec3& view_forward) {
  if (axis == ManipulatorAxis::trans_c) {
    return free_delta;
  }

  if (axis == ManipulatorAxis::trans_xy) {
    return projectOntoPlaneAxes(free_delta, basis.axis_x, basis.axis_y);
  }
  if (axis == ManipulatorAxis::trans_yz) {
    return projectOntoPlaneAxes(free_delta, basis.axis_y, basis.axis_z);
  }
  if (axis == ManipulatorAxis::trans_zx) {
    return projectOntoPlaneAxes(free_delta, basis.axis_z, basis.axis_x);
  }

  const glm::vec3 axis_dir =
      normalizedOr(axisDirectionFor(axis, basis), glm::vec3(1.0f, 0.0f, 0.0f));
  const glm::vec3 view_dir =
      normalizedOr(view_forward, glm::vec3(0.0f, 0.0f, -1.0f));
  if (std::abs(glm::dot(axis_dir, view_dir)) > 0.98f) {
    return free_delta - axis_dir * glm::dot(free_delta, axis_dir);
  }
  return axis_dir * glm::dot(free_delta, axis_dir);
}

bool translateSessionShowsPlaneHandle(const ManipulatorAxis active,
                                      const ManipulatorAxis plane) {
  if (!isPlaneManipulator(plane)) {
    return false;
  }
  if (isPlaneManipulator(active)) {
    return active == plane;
  }
  return active == ManipulatorAxis::last;
}

bool translateSessionShowsCenterHandle() {
  return false;
}

uint32_t translateSessionGuideAxisCount(const ManipulatorAxis active) {
  if (active == ManipulatorAxis::trans_c) {
    return 0u;
  }
  if (isPlaneManipulator(active)) {
    return 2u;
  }
  return isTranslationManipulator(active) ? 1u : 0u;
}

ManipulatorAxis translateSessionOriginColorAxis(const ManipulatorAxis active) {
  switch (active) {
    case ManipulatorAxis::trans_xy:
      return ManipulatorAxis::trans_z;
    case ManipulatorAxis::trans_yz:
      return ManipulatorAxis::trans_x;
    case ManipulatorAxis::trans_zx:
      return ManipulatorAxis::trans_y;
    case ManipulatorAxis::trans_x:
    case ManipulatorAxis::trans_y:
    case ManipulatorAxis::trans_z:
      return active;
    default:
      return ManipulatorAxis::last;
  }
}

float viewportHeightWorldPerPixel(const EditorCamera& camera,
                                  const glm::vec3& pivot_position) {
  const float height = std::max(camera.getViewportHeight(), 1.0f);
  if (camera.getProjectionMode() == EditorCamera::ProjectionMode::orthographic) {
    return camera.getOrthoSize() / height;
  }
  const glm::vec3 view_forward =
      normalizedOr(camera.getForwardDirection(), glm::vec3(0.0f, 0.0f, -1.0f));
  const float pivot_depth = std::max(
      std::abs(glm::dot(pivot_position - glm::vec3(camera.getPosition()),
                        view_forward)),
      camera.getNearClip());
  const float visible_height =
      2.0f * pivot_depth * std::tan(camera.getVerticalFov() * 0.5f);
  return visible_height / height;
}

float viewportHeightWorldPerPixel(const EditorCamera& camera) {
  return viewportHeightWorldPerPixel(camera, camera.getFocalPoint());
}

TranslateModalCameraState cameraStateFromEditorCamera(
    const EditorCamera& camera, const glm::vec3& pivot_position) {
  TranslateModalCameraState state{};
  state.position = camera.getPosition();
  state.forward = camera.getForwardDirection();
  state.right = camera.getRightDirection();
  state.up = camera.getUpDirection();
  state.viewport_height_world_per_pixel =
      viewportHeightWorldPerPixel(camera, pivot_position);
  return state;
}

TranslateModalCameraState cameraStateFromEditorCamera(const EditorCamera& camera) {
  return cameraStateFromEditorCamera(camera, camera.getFocalPoint());
}

void TranslateModalSession::beginFromHandle(
    const ManipulatorAxis axis, const GizmoBasis& basis,
    const glm::vec2& pointer_position, const glm::vec3& object_position,
    const TranslateModalCameraState& camera) {
  m_axis = axis;
  m_basis = basis;
  m_camera = camera;
  m_start_pointer = pointer_position;
  m_object_position_at_begin = object_position;
  m_feedback_delta = glm::vec3(0.0f);
  m_active = true;
}

void TranslateModalSession::beginFromHandle(
    const ManipulatorAxis axis, const GizmoBasis& basis,
    const glm::vec2& pointer_position, const glm::vec3& object_position,
    const EditorCamera& camera) {
  // Depth scaling uses the world-space gizmo pivot; object_position remains
  // the local/entity-space translation baseline for feedbackPosition().
  beginFromHandle(axis, basis, pointer_position, object_position,
                  cameraStateFromEditorCamera(camera, basis.origin));
}

void TranslateModalSession::onPointerMove(
    const glm::vec2& pointer_position, const TranslateModalCameraState& camera) {
  if (!m_active) {
    return;
  }
  m_camera = camera;
  const glm::vec2 pixel_delta = pointer_position - m_start_pointer;
  const glm::vec3 free_delta = screenDeltaToViewPlaneWorld(
      m_camera.position, m_camera.forward, m_camera.right, m_camera.up,
      m_camera.viewport_height_world_per_pixel, pixel_delta);
  m_feedback_delta =
      constrainTranslationDelta(free_delta, m_axis, m_basis, m_camera.forward);
}

void TranslateModalSession::onPointerMove(const glm::vec2& pointer_position,
                                          const EditorCamera& camera) {
  onPointerMove(pointer_position,
                cameraStateFromEditorCamera(camera, m_basis.origin));
}

std::optional<glm::vec3> TranslateModalSession::confirm() {
  if (!m_active) {
    return std::nullopt;
  }
  const glm::vec3 result = feedbackPosition();
  m_active = false;
  return result;
}

void TranslateModalSession::cancel() {
  m_active = false;
  m_feedback_delta = glm::vec3(0.0f);
}

bool TranslateModalSession::isActive() const {
  return m_active;
}

ManipulatorAxis TranslateModalSession::activeHandle() const {
  return m_active ? m_axis : ManipulatorAxis::last;
}

const GizmoBasis& TranslateModalSession::dragStartBasis() const {
  return m_basis;
}

glm::vec3 TranslateModalSession::feedbackDelta() const {
  return m_feedback_delta;
}

glm::vec3 TranslateModalSession::feedbackPosition() const {
  return m_object_position_at_begin + m_feedback_delta;
}

}  // namespace Blunder
