#include "runtime/function/render/gizmo/translate_modal_session.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

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

GizmoBasis worldBasisAt(const glm::vec3& origin) {
  GizmoBasis basis{};
  basis.origin = origin;
  basis.axis_x = glm::vec3(1.0f, 0.0f, 0.0f);
  basis.axis_y = glm::vec3(0.0f, 1.0f, 0.0f);
  basis.axis_z = glm::vec3(0.0f, 0.0f, 1.0f);
  return basis;
}

glm::vec3 rotateAxis(const glm::quat& rotation, const glm::vec3& axis) {
  return normalizedOr(rotation * axis, axis);
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
  return active == ManipulatorAxis::trans_c || active == ManipulatorAxis::last;
}

bool translateSessionShowsCenterHandle() {
  return false;
}

uint32_t translateSessionGuideAxisCount(const ManipulatorAxis active) {
  ManipulatorAxis axes[2] = {ManipulatorAxis::last, ManipulatorAxis::last};
  return translateSessionGuideAxes(active, axes);
}

uint32_t translateSessionGuideAxes(const ManipulatorAxis active,
                                   ManipulatorAxis out_axes[2]) {
  switch (active) {
    case ManipulatorAxis::trans_x:
    case ManipulatorAxis::trans_y:
    case ManipulatorAxis::trans_z:
      out_axes[0] = active;
      out_axes[1] = ManipulatorAxis::last;
      return 1u;
    case ManipulatorAxis::trans_xy:
      out_axes[0] = ManipulatorAxis::trans_x;
      out_axes[1] = ManipulatorAxis::trans_y;
      return 2u;
    case ManipulatorAxis::trans_yz:
      out_axes[0] = ManipulatorAxis::trans_y;
      out_axes[1] = ManipulatorAxis::trans_z;
      return 2u;
    case ManipulatorAxis::trans_zx:
      out_axes[0] = ManipulatorAxis::trans_z;
      out_axes[1] = ManipulatorAxis::trans_x;
      return 2u;
    default:
      out_axes[0] = ManipulatorAxis::last;
      out_axes[1] = ManipulatorAxis::last;
      return 0u;
  }
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

bool translateModalConfirmsOnMousePress(const TranslateModalEntry entry) {
  return entry == TranslateModalEntry::grab;
}

bool translateModalConfirmsOnMouseRelease(const TranslateModalEntry entry) {
  return entry == TranslateModalEntry::handle;
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
    const TranslateModalCameraState& camera,
    const glm::quat& session_start_rotation,
    const TranslateModalConstraintOrientation initial_orientation) {
  m_entry = TranslateModalEntry::handle;
  m_axis = axis;
  m_basis = basis;
  m_camera = camera;
  m_start_pointer = pointer_position;
  m_current_pointer = pointer_position;
  m_object_position_at_begin = object_position;
  m_session_start_rotation = session_start_rotation;
  m_feedback_delta = glm::vec3(0.0f);
  m_active_slot = constraintSlotForManipulator(axis);
  m_orientation = initial_orientation;
  m_numeric_buffer.clear();
  if (m_axis != ManipulatorAxis::trans_c &&
      m_orientation == TranslateModalConstraintOrientation::local) {
    rebuildConstraintBasis();
  }
  m_active = true;
}

void TranslateModalSession::beginFromHandle(
    const ManipulatorAxis axis, const GizmoBasis& basis,
    const glm::vec2& pointer_position, const glm::vec3& object_position,
    const EditorCamera& camera, const glm::quat& session_start_rotation,
    const TranslateModalConstraintOrientation initial_orientation) {
  // Depth scaling uses the world-space gizmo pivot; object_position remains
  // the local/entity-space translation baseline for feedbackPosition().
  beginFromHandle(axis, basis, pointer_position, object_position,
                  cameraStateFromEditorCamera(camera, basis.origin),
                  session_start_rotation, initial_orientation);
}

void TranslateModalSession::beginFromGrab(
    const glm::vec2& pointer_position, const glm::vec3& object_position,
    const TranslateModalCameraState& camera,
    const glm::quat& session_start_rotation) {
  m_entry = TranslateModalEntry::grab;
  m_axis = ManipulatorAxis::trans_c;
  m_basis = worldBasisAt(object_position);
  m_camera = camera;
  m_start_pointer = pointer_position;
  m_current_pointer = pointer_position;
  m_object_position_at_begin = object_position;
  m_session_start_rotation = session_start_rotation;
  m_feedback_delta = glm::vec3(0.0f);
  m_active_slot = TranslateModalConstraintSlot::none;
  m_orientation = TranslateModalConstraintOrientation::global;
  m_numeric_buffer.clear();
  m_active = true;
}

void TranslateModalSession::beginFromGrab(
    const glm::vec2& pointer_position, const glm::vec3& object_position,
    const EditorCamera& camera, const glm::quat& session_start_rotation) {
  beginFromGrab(pointer_position, object_position,
                cameraStateFromEditorCamera(camera, object_position),
                session_start_rotation);
}

TranslateModalSession::TranslateModalConstraintSlot
TranslateModalSession::axisSlotFor(const TranslateModalAxisKey key) {
  switch (key) {
    case TranslateModalAxisKey::x:
      return TranslateModalConstraintSlot::axis_x;
    case TranslateModalAxisKey::y:
      return TranslateModalConstraintSlot::axis_y;
    case TranslateModalAxisKey::z:
      return TranslateModalConstraintSlot::axis_z;
  }
  return TranslateModalConstraintSlot::none;
}

TranslateModalSession::TranslateModalConstraintSlot
TranslateModalSession::planeSlotFor(const TranslateModalAxisKey key) {
  switch (key) {
    case TranslateModalAxisKey::x:
      return TranslateModalConstraintSlot::plane_lock_x;
    case TranslateModalAxisKey::y:
      return TranslateModalConstraintSlot::plane_lock_y;
    case TranslateModalAxisKey::z:
      return TranslateModalConstraintSlot::plane_lock_z;
  }
  return TranslateModalConstraintSlot::none;
}

TranslateModalSession::TranslateModalConstraintSlot
TranslateModalSession::constraintSlotForManipulator(const ManipulatorAxis axis) {
  switch (axis) {
    case ManipulatorAxis::trans_x:
      return TranslateModalConstraintSlot::axis_x;
    case ManipulatorAxis::trans_y:
      return TranslateModalConstraintSlot::axis_y;
    case ManipulatorAxis::trans_z:
      return TranslateModalConstraintSlot::axis_z;
    case ManipulatorAxis::trans_xy:
      return TranslateModalConstraintSlot::plane_lock_z;
    case ManipulatorAxis::trans_yz:
      return TranslateModalConstraintSlot::plane_lock_x;
    case ManipulatorAxis::trans_zx:
      return TranslateModalConstraintSlot::plane_lock_y;
    default:
      return TranslateModalConstraintSlot::none;
  }
}

ManipulatorAxis TranslateModalSession::manipulatorAxisFor(
    const TranslateModalAxisKey key) {
  switch (key) {
    case TranslateModalAxisKey::x:
      return ManipulatorAxis::trans_x;
    case TranslateModalAxisKey::y:
      return ManipulatorAxis::trans_y;
    case TranslateModalAxisKey::z:
      return ManipulatorAxis::trans_z;
  }
  return ManipulatorAxis::last;
}

ManipulatorAxis TranslateModalSession::manipulatorPlaneFor(
    const TranslateModalAxisKey locked_axis) {
  switch (locked_axis) {
    case TranslateModalAxisKey::x:
      return ManipulatorAxis::trans_yz;
    case TranslateModalAxisKey::y:
      return ManipulatorAxis::trans_zx;
    case TranslateModalAxisKey::z:
      return ManipulatorAxis::trans_xy;
  }
  return ManipulatorAxis::last;
}

void TranslateModalSession::rebuildConstraintBasis() {
  const glm::vec3 origin = m_basis.origin;
  if (m_orientation == TranslateModalConstraintOrientation::global) {
    m_basis.axis_x = glm::vec3(1.0f, 0.0f, 0.0f);
    m_basis.axis_y = glm::vec3(0.0f, 1.0f, 0.0f);
    m_basis.axis_z = glm::vec3(0.0f, 0.0f, 1.0f);
  } else {
    m_basis.axis_x = rotateAxis(m_session_start_rotation, glm::vec3(1.0f, 0.0f, 0.0f));
    m_basis.axis_y = rotateAxis(m_session_start_rotation, glm::vec3(0.0f, 1.0f, 0.0f));
    m_basis.axis_z = rotateAxis(m_session_start_rotation, glm::vec3(0.0f, 0.0f, 1.0f));
  }
  m_basis.origin = origin;
}

void TranslateModalSession::reprojectFeedbackFromPointer() {
  const glm::vec2 pixel_delta = m_current_pointer - m_start_pointer;
  const glm::vec3 free_delta = screenDeltaToViewPlaneWorld(
      m_camera.position, m_camera.forward, m_camera.right, m_camera.up,
      m_camera.viewport_height_world_per_pixel, pixel_delta);
  m_feedback_delta =
      constrainTranslationDelta(free_delta, m_axis, m_basis, m_camera.forward);
}

float TranslateModalSession::parseNumericBuffer() const {
  if (m_numeric_buffer.empty() || m_numeric_buffer == "-" ||
      m_numeric_buffer == "." || m_numeric_buffer == "-.") {
    return 0.0f;
  }
  char* end = nullptr;
  const float value =
      std::strtof(m_numeric_buffer.c_str(), &end);
  if (end == m_numeric_buffer.c_str()) {
    return 0.0f;
  }
  return value;
}

glm::vec3 TranslateModalSession::numericConstraintDirection() const {
  if (m_axis == ManipulatorAxis::trans_x ||
      m_axis == ManipulatorAxis::trans_y ||
      m_axis == ManipulatorAxis::trans_z) {
    return normalizedOr(axisDirectionFor(m_axis, m_basis),
                        glm::vec3(1.0f, 0.0f, 0.0f));
  }

  const glm::vec2 pixel_delta = m_current_pointer - m_start_pointer;
  const glm::vec3 free_delta = screenDeltaToViewPlaneWorld(
      m_camera.position, m_camera.forward, m_camera.right, m_camera.up,
      m_camera.viewport_height_world_per_pixel, pixel_delta);
  const glm::vec3 constrained = constrainTranslationDelta(
      free_delta, m_axis, m_basis, m_camera.forward);
  return normalizedOr(constrained,
                      normalizedOr(m_camera.right, glm::vec3(1.0f, 0.0f, 0.0f)));
}

void TranslateModalSession::applyNumericDistance() {
  m_feedback_delta = numericConstraintDirection() * parseNumericBuffer();
}

void TranslateModalSession::exitNumericMode() {
  m_numeric_buffer.clear();
  reprojectFeedbackFromPointer();
}

bool TranslateModalSession::appendNumericChar(const char c) {
  if (!m_active) {
    return false;
  }

  if (c >= '0' && c <= '9') {
    m_numeric_buffer.push_back(c);
    applyNumericDistance();
    return true;
  }

  if (c == '-') {
    if (!m_numeric_buffer.empty()) {
      return false;
    }
    m_numeric_buffer.push_back(c);
    applyNumericDistance();
    return true;
  }

  if (c == '.') {
    if (m_numeric_buffer.find('.') != std::string::npos) {
      return false;
    }
    m_numeric_buffer.push_back(c);
    applyNumericDistance();
    return true;
  }

  return false;
}

bool TranslateModalSession::backspaceNumeric() {
  if (!m_active || m_numeric_buffer.empty()) {
    return false;
  }
  m_numeric_buffer.pop_back();
  if (m_numeric_buffer.empty()) {
    exitNumericMode();
  } else {
    applyNumericDistance();
  }
  return true;
}

void TranslateModalSession::clearNumeric() {
  if (m_numeric_buffer.empty()) {
    return;
  }
  exitNumericMode();
}

bool TranslateModalSession::hasNumericInput() const {
  return !m_numeric_buffer.empty();
}

const std::string& TranslateModalSession::numericBuffer() const {
  return m_numeric_buffer;
}

void TranslateModalSession::setGlobalAxisConstraint(
    const TranslateModalAxisKey key) {
  m_active_slot = axisSlotFor(key);
  m_orientation = TranslateModalConstraintOrientation::global;
  m_axis = manipulatorAxisFor(key);
  rebuildConstraintBasis();
}

void TranslateModalSession::setGlobalPlaneConstraint(
    const TranslateModalAxisKey locked_axis) {
  m_active_slot = planeSlotFor(locked_axis);
  m_orientation = TranslateModalConstraintOrientation::global;
  m_axis = manipulatorPlaneFor(locked_axis);
  rebuildConstraintBasis();
}

void TranslateModalSession::advanceConstraintCycle(
    const TranslateModalConstraintSlot slot, const ManipulatorAxis axis) {
  if (m_active_slot == slot && m_axis == axis &&
      m_orientation == TranslateModalConstraintOrientation::global) {
    m_orientation = TranslateModalConstraintOrientation::local;
    rebuildConstraintBasis();
    return;
  }
  if (m_active_slot == slot && m_axis == axis &&
      m_orientation == TranslateModalConstraintOrientation::local) {
    m_axis = ManipulatorAxis::trans_c;
    m_active_slot = TranslateModalConstraintSlot::none;
    m_orientation = TranslateModalConstraintOrientation::global;
    m_basis = worldBasisAt(m_basis.origin);
    return;
  }
  if (slot == TranslateModalConstraintSlot::axis_x ||
      slot == TranslateModalConstraintSlot::axis_y ||
      slot == TranslateModalConstraintSlot::axis_z) {
    setGlobalAxisConstraint(slot == TranslateModalConstraintSlot::axis_x
                                ? TranslateModalAxisKey::x
                                : slot == TranslateModalConstraintSlot::axis_y
                                      ? TranslateModalAxisKey::y
                                      : TranslateModalAxisKey::z);
    return;
  }
  setGlobalPlaneConstraint(
      slot == TranslateModalConstraintSlot::plane_lock_x
          ? TranslateModalAxisKey::x
          : slot == TranslateModalConstraintSlot::plane_lock_y
                ? TranslateModalAxisKey::y
                : TranslateModalAxisKey::z);
}

void TranslateModalSession::applyAxisConstraintKey(
    const TranslateModalAxisKey key) {
  if (!m_active) {
    return;
  }
  const TranslateModalConstraintSlot slot = axisSlotFor(key);
  const ManipulatorAxis axis = manipulatorAxisFor(key);
  advanceConstraintCycle(slot, axis);
  if (hasNumericInput()) {
    applyNumericDistance();
  } else {
    reprojectFeedbackFromPointer();
  }
}

void TranslateModalSession::applyPlaneConstraintKey(
    const TranslateModalAxisKey locked_axis) {
  if (!m_active) {
    return;
  }
  const TranslateModalConstraintSlot slot = planeSlotFor(locked_axis);
  const ManipulatorAxis axis = manipulatorPlaneFor(locked_axis);
  advanceConstraintCycle(slot, axis);
  if (hasNumericInput()) {
    applyNumericDistance();
  } else {
    reprojectFeedbackFromPointer();
  }
}

void TranslateModalSession::onPointerMove(
    const glm::vec2& pointer_position, const TranslateModalCameraState& camera) {
  if (!m_active) {
    return;
  }
  m_camera = camera;
  m_current_pointer = pointer_position;
  if (!hasNumericInput()) {
    reprojectFeedbackFromPointer();
  }
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
  m_numeric_buffer.clear();
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

TranslateModalEntry TranslateModalSession::entryKind() const {
  return m_entry;
}

bool TranslateModalSession::isGrabEntry() const {
  return m_entry == TranslateModalEntry::grab;
}

TranslateModalConstraintOrientation TranslateModalSession::constraintOrientation()
    const {
  return m_orientation;
}

bool TranslateModalSession::isConstraintFree() const {
  return m_active && m_axis == ManipulatorAxis::trans_c;
}

}  // namespace Blunder
