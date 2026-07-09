#include "runtime/function/render/gizmo/transform_gizmo_controller.h"

#include <SDL3/SDL.h>

#include "runtime/core/event/event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/gizmo/transform_gizmo_pick.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/slint/slint_system.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

namespace {

SceneInstance* activeScene() {
  if (!g_runtime_global_context.m_scene_system) {
    return nullptr;
  }
  return g_runtime_global_context.m_scene_system->getActiveInstance();
}

Entity* selectedEntity() {
  if (!g_runtime_global_context.m_editor_selection) {
    return nullptr;
  }
  SceneInstance* scene = activeScene();
  if (!scene || !g_runtime_global_context.m_editor_selection->hasSelection()) {
    return nullptr;
  }
  return scene->getEntity(g_runtime_global_context.m_editor_selection->getSelection());
}

void syncInspectorLive() {
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->syncInspectorFromSelection();
  }
}

void syncToolbar() {
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->syncTransformToolbarFromEngine();
  }
}

void markSceneDirty() {
  if (SceneInstance* scene = activeScene()) {
    scene->markTransformsDirty();
  }
  if (g_runtime_global_context.m_editor_scene_edit) {
    g_runtime_global_context.m_editor_scene_edit->markDirty();
  }
  if (g_runtime_global_context.m_render_system) {
    g_runtime_global_context.m_render_system->requestViewportRedraw();
  }
}

EntityId selectedEntityId() {
  if (!g_runtime_global_context.m_editor_selection ||
      !g_runtime_global_context.m_editor_selection->hasSelection()) {
    return k_invalid_entity_id;
  }
  return g_runtime_global_context.m_editor_selection->getSelection();
}

Vec3 localPositionForWorldPosition(const SceneInstance& scene,
                                   const Entity& entity,
                                   const glm::vec3& world_position) {
  Mat4 parent_world = scene.getSceneToWorldMatrix();
  const EntityId parent_id = entity.getParentId();
  if (isValid(parent_id)) {
    parent_world = scene.getWorldMatrix(parent_id);
  }

  const Vec4 local_position =
      glm::inverse(parent_world) * Vec4(world_position, 1.0f);
  return Vec3(local_position);
}

}  // namespace

void TransformGizmoController::endCameraLock(EditorCamera* camera) {
  if (m_locked_camera != nullptr) {
    m_locked_camera->setInteractionLocked(false);
    m_locked_camera = nullptr;
  } else if (camera != nullptr) {
    camera->setInteractionLocked(false);
  }
}

void TransformGizmoController::setTranslateModalCursor() {
  if (g_runtime_global_context.m_window_system) {
    g_runtime_global_context.m_window_system->setSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
  }
}

void TransformGizmoController::clearTranslateModalCursor() {
  if (g_runtime_global_context.m_window_system) {
    g_runtime_global_context.m_window_system->clearSystemCursor();
  }
}

void TransformGizmoController::requestViewportRedraw() const {
  if (g_runtime_global_context.m_render_system) {
    g_runtime_global_context.m_render_system->requestViewportRedraw();
  }
}

void TransformGizmoController::setMode(const TransformGizmoMode mode,
                                       EditorCamera* camera) {
  if (m_mode == mode) {
    return;
  }
  cancelInteraction(camera);
  m_mode = mode;
  requestViewportRedraw();
}

void TransformGizmoController::toggleSpace() {
  m_space = (m_space == GizmoSpace::global) ? GizmoSpace::local : GizmoSpace::global;
  requestViewportRedraw();
}

void TransformGizmoController::cancelInteraction(EditorCamera* camera) {
  if (m_translate_session.isActive()) {
    m_translate_session.cancel();
    clearTranslateModalCursor();
    requestViewportRedraw();
  }
  m_active_axis.reset();
  endCameraLock(camera);
}

void TransformGizmoController::restoreEntityTransformAtDragStart(
    Entity& entity) const {
  entity.setPosition(m_entity_position_at_drag_start);
  entity.setRotation(m_entity_rotation_at_drag_start);
  entity.setScale(m_entity_scale_at_drag_start);
}

bool TransformGizmoController::beginGrabFromSelection(EditorCamera& camera) {
  if (m_translate_session.isActive() || isDragging()) {
    return false;
  }

  SceneInstance* scene = activeScene();
  Entity* entity = selectedEntity();
  const EntityId entity_id = selectedEntityId();
  if (!scene || !entity || !isValid(entity_id)) {
    return false;
  }

  const glm::mat4 world = scene->getWorldMatrix(entity_id);
  const GizmoBasis basis = buildGizmoBasis(world, m_space);
  float mouse_x = 0.0f;
  float mouse_y = 0.0f;
  SDL_GetMouseState(&mouse_x, &mouse_y);
  const Vec2 window_pos(mouse_x, mouse_y);
  if (!camera.isWindowPositionInViewport(window_pos)) {
    return false;
  }

  m_drag_basis = basis;
  m_entity_position_at_drag_start = entity->getPosition();
  m_entity_rotation_at_drag_start = entity->getRotation();
  m_entity_scale_at_drag_start = entity->getScale();
  m_translate_session.beginFromGrab(window_pos, basis.origin,
                                    cameraStateFromEditorCamera(camera, basis.origin));

  m_locked_camera = &camera;
  camera.setInteractionLocked(true);
  setTranslateModalCursor();
  markSceneDirty();
  syncInspectorLive();
  return true;
}

bool TransformGizmoController::confirmTranslateModalSession(EditorCamera& camera) {
  if (!m_translate_session.isActive()) {
    return false;
  }

  SceneInstance* scene = activeScene();
  Entity* entity = selectedEntity();
  const bool was_grab_entry = m_translate_session.isGrabEntry();
  if (entity != nullptr && scene != nullptr) {
    if (const std::optional<glm::vec3> confirmed =
            m_translate_session.confirm()) {
      if (was_grab_entry) {
        entity->setPosition(localPositionForWorldPosition(*scene, *entity,
                                                          *confirmed));
      } else {
        entity->setPosition(Vec3(*confirmed));
      }
    }
  } else {
    m_translate_session.cancel();
  }

  clearTranslateModalCursor();
  m_active_axis.reset();
  endCameraLock(&camera);
  markSceneDirty();
  syncInspectorLive();
  return true;
}

bool TransformGizmoController::cancelTranslateModalSession(EditorCamera& camera) {
  if (!m_translate_session.isActive()) {
    return false;
  }

  if (Entity* entity = selectedEntity()) {
    restoreEntityTransformAtDragStart(*entity);
  }
  m_translate_session.cancel();
  clearTranslateModalCursor();
  m_active_axis.reset();
  endCameraLock(&camera);
  markSceneDirty();
  syncInspectorLive();
  return true;
}

bool TransformGizmoController::buildActiveGizmoBasis(GizmoBasis& out_basis) const {
  const bool grab_translate_session =
      m_translate_session.isActive() && m_translate_session.isGrabEntry();
  if (m_mode != TransformGizmoMode::translate &&
      m_mode != TransformGizmoMode::rotate &&
      m_mode != TransformGizmoMode::scale &&
      !grab_translate_session) {
    return false;
  }
  if (!g_runtime_global_context.m_editor_selection ||
      !g_runtime_global_context.m_editor_selection->hasSelection()) {
    return false;
  }
  const Entity* entity = selectedEntity();
  SceneInstance* scene = activeScene();
  if (!entity || !scene) {
    return false;
  }
  const glm::mat4 world = scene->getWorldMatrix(
      g_runtime_global_context.m_editor_selection->getSelection());
  out_basis = buildGizmoBasis(world, m_space);
  return true;
}

void TransformGizmoController::onEvent(Event& event, EditorCamera& camera) {
  if (event.handled) {
    return;
  }

  switch (event.getEventType()) {
    case EventType::KeyPressed:
      if (onKeyPressed(event, camera)) {
        event.handled = true;
      }
      break;
    case EventType::MouseButtonPressed:
      if (onMousePressed(event, camera)) {
        event.handled = true;
      }
      break;
    case EventType::MouseButtonReleased:
      if (onMouseReleased(event, camera)) {
        event.handled = true;
      }
      break;
    case EventType::MouseMoved:
      if (onMouseMoved(event, camera)) {
        event.handled = true;
      }
      break;
    default:
      break;
  }
}

bool TransformGizmoController::onKeyPressed(Event& event, EditorCamera& camera) {
  auto& key_event = static_cast<KeyPressedEvent&>(event);
  if (key_event.isRepeat()) {
    return false;
  }

  switch (key_event.getKeyCode()) {
    case SDLK_G:
      return beginGrabFromSelection(camera);
    case SDLK_R:
      setMode(TransformGizmoMode::rotate, &camera);
      syncToolbar();
      return true;
    case SDLK_S:
      if (!key_event.isCtrlDown()) {
        setMode(TransformGizmoMode::scale, &camera);
        syncToolbar();
        return true;
      }
      return false;
    case SDLK_ESCAPE:
      if (m_translate_session.isActive()) {
        return cancelTranslateModalSession(camera);
      }
      if (m_active_axis) {
        if (Entity* entity = selectedEntity()) {
          if (m_mode == TransformGizmoMode::rotate &&
              isRotationManipulator(*m_active_axis)) {
            entity->setRotation(m_entity_rotation_at_drag_start);
          } else if (m_mode == TransformGizmoMode::scale &&
                     isScaleManipulator(*m_active_axis)) {
            entity->setScale(m_entity_scale_at_drag_start);
          } else {
            restoreEntityTransformAtDragStart(*entity);
          }
          markSceneDirty();
          syncInspectorLive();
        }
        cancelInteraction(&camera);
        return true;
      }
      setMode(TransformGizmoMode::none, &camera);
      syncToolbar();
      return true;
    case SDLK_COMMA:
      toggleSpace();
      syncToolbar();
      return true;
    default:
      return false;
  }
}

bool TransformGizmoController::onMousePressed(Event& event, EditorCamera& camera) {
  auto& mouse = static_cast<MouseButtonPressedEvent&>(event);
  if (mouse.getMouseButton() == SDL_BUTTON_RIGHT &&
      m_translate_session.isActive()) {
    return cancelTranslateModalSession(camera);
  }
  if (mouse.getMouseButton() == SDL_BUTTON_LEFT &&
      m_translate_session.isActive() &&
      translateModalConfirmsOnMousePress(m_translate_session.entryKind())) {
    if (!mouse.hasMousePosition()) {
      return false;
    }
    const Vec2 window_pos(mouse.getX(), mouse.getY());
    if (!camera.isWindowPositionInViewport(window_pos)) {
      return false;
    }
    return confirmTranslateModalSession(camera);
  }
  if (mouse.getMouseButton() != SDL_BUTTON_LEFT || !mouse.hasMousePosition()) {
    return false;
  }
  if (m_mode != TransformGizmoMode::translate &&
      m_mode != TransformGizmoMode::rotate &&
      m_mode != TransformGizmoMode::scale) {
    return false;
  }

  const Vec2 window_pos(mouse.getX(), mouse.getY());
  if (!camera.isWindowPositionInViewport(window_pos)) {
    return false;
  }

  GizmoBasis basis;
  if (!buildActiveGizmoBasis(basis)) {
    return false;
  }

  const Ray ray = camera.makeRayFromWindowPosition(window_pos);
  TransformGizmoScaleContext scale_ctx{};
  scale_ctx.view_projection = camera.getProjectionMatrix() * camera.getViewMatrix();
  scale_ctx.pivot = basis.origin;
  scale_ctx.viewport_height = camera.getViewportHeight();
  scale_ctx.is_perspective =
      camera.getProjectionMode() == EditorCamera::ProjectionMode::perspective;
  scale_ctx.ortho_size = camera.getOrthoSize();
  const float group_scale = computeGizmoGroupScale(scale_ctx);
  m_gizmo_group_scale = group_scale;
  m_drag_view_normal =
      glm::normalize(basis.origin - glm::vec3(camera.getPosition()));

  TransformGizmoPickContext pick_ctx{};
  pick_ctx.ray = ray;
  pick_ctx.basis = basis;
  pick_ctx.group_scale = group_scale;
  pick_ctx.camera_forward = camera.getForwardDirection();
  pick_ctx.camera_position = camera.getPosition();

  std::optional<ManipulatorAxis> hit;
  if (m_mode == TransformGizmoMode::translate) {
    hit = pickTranslationGizmoHandle(pick_ctx);
  } else if (m_mode == TransformGizmoMode::rotate) {
    hit = pickRotationGizmoHandle(pick_ctx);
  } else {
    hit = pickScaleGizmoHandle(pick_ctx);
  }
  if (!hit) {
    return false;
  }

  Entity* entity = selectedEntity();
  if (!entity) {
    return false;
  }

  m_active_axis = hit;
  m_drag_basis = basis;
  m_entity_position_at_drag_start = entity->getPosition();
  m_entity_rotation_at_drag_start = entity->getRotation();
  m_entity_scale_at_drag_start = entity->getScale();

  if (m_mode == TransformGizmoMode::translate && isTranslationManipulator(*hit)) {
    m_translate_session.beginFromHandle(*hit, basis, window_pos,
                                        entity->getPosition(), camera);
    setTranslateModalCursor();
    requestViewportRedraw();
  } else if (m_mode == TransformGizmoMode::scale && isScaleManipulator(*hit)) {
    if (*hit == ManipulatorAxis::trans_c) {
      const Plane view_plane =
          Plane::fromPointAndNormal(basis.origin, m_drag_view_normal);
      const auto start_pt = intersectRayPlane(ray, view_plane);
      m_drag_start_world = start_pt.value_or(basis.origin);
      m_scale_drag_start_param =
          glm::length(m_drag_start_world - basis.origin);
    } else {
      glm::vec3 axis_dir = basis.axis_x;
      if (*hit == ManipulatorAxis::trans_y) {
        axis_dir = basis.axis_y;
      } else if (*hit == ManipulatorAxis::trans_z) {
        axis_dir = basis.axis_z;
      }
      axis_dir = glm::normalize(axis_dir);
      const auto t = rayLineParameter(ray, basis.origin, axis_dir);
      const glm::vec3 start_pt =
          t ? closestPointOnLine(basis.origin, axis_dir, ray.pointAt(*t))
            : scaleHandleWorldCenter(
                  basis, *hit,
                  computeGizmoHandleScale(m_gizmo_group_scale, *hit));
      m_drag_start_world = start_pt;
      m_scale_drag_start_param = glm::dot(start_pt - basis.origin, axis_dir);
      if (std::abs(m_scale_drag_start_param) < 1e-4f) {
        m_scale_drag_start_param =
            computeGizmoHandleScale(m_gizmo_group_scale, *hit) *
            TransformGizmoMetrics::k_mesh_scale_box_center_offset;
      }
    }
  } else {
    const glm::vec3 rot_axis = rotationAxisFor(*hit, basis);
    const auto plane_hit = intersectRayWithAxisPlane(ray, basis.origin, rot_axis);
    if (!plane_hit) {
      m_active_axis.reset();
      return false;
    }
    m_rotation_drag_reference = *plane_hit;
    m_rotation_arc_mesh_base =
        rotationDialMeshAngle(basis, *hit, m_rotation_drag_reference);
    m_drag_start_angle =
        signedAngleOnRotationPlane(rot_axis, basis.origin, m_rotation_drag_reference,
                                   *plane_hit);
    m_drag_current_angle = m_drag_start_angle;
  }

  m_locked_camera = &camera;
  camera.setInteractionLocked(true);
  return true;
}

bool TransformGizmoController::onMouseReleased(Event& event, EditorCamera& camera) {
  auto& mouse = static_cast<MouseButtonReleasedEvent&>(event);
  if (mouse.getMouseButton() != SDL_BUTTON_LEFT || !m_active_axis) {
    return false;
  }

  if (m_translate_session.isActive() &&
      translateModalConfirmsOnMouseRelease(m_translate_session.entryKind())) {
    return confirmTranslateModalSession(camera);
  }
  m_active_axis.reset();
  endCameraLock(&camera);
  markSceneDirty();
  return true;
}

bool TransformGizmoController::onMouseMoved(Event& event, EditorCamera& camera) {
  if (!m_active_axis && !m_translate_session.isActive()) {
    return false;
  }

  auto& mouse = static_cast<MouseMovedEvent&>(event);
  const Vec2 window_pos(mouse.getX(), mouse.getY());
  if (!camera.isWindowPositionInViewport(window_pos)) {
    return true;
  }

  Entity* entity = selectedEntity();
  SceneInstance* scene = activeScene();
  if (!entity || !scene ||
      !g_runtime_global_context.m_editor_selection ||
      !g_runtime_global_context.m_editor_selection->hasSelection()) {
    return true;
  }

  if (m_translate_session.isActive()) {
    const bool is_grab_entry = m_translate_session.isGrabEntry();
    m_translate_session.onPointerMove(window_pos, camera);
    if (is_grab_entry) {
      entity->setPosition(localPositionForWorldPosition(
          *scene, *entity, m_translate_session.feedbackPosition()));
    } else {
      entity->setPosition(Vec3(m_translate_session.feedbackPosition()));
    }
  } else if (m_mode == TransformGizmoMode::rotate &&
             isRotationManipulator(*m_active_axis)) {
    const Ray ray = camera.makeRayFromWindowPosition(window_pos);
    const glm::vec3 rot_axis = rotationAxisFor(*m_active_axis, m_drag_basis);
    const auto plane_hit = intersectRayWithAxisPlane(ray, m_drag_basis.origin, rot_axis);
    if (!plane_hit) {
      return true;
    }
    m_drag_current_angle = signedAngleOnRotationPlane(
        rot_axis, m_drag_basis.origin, m_rotation_drag_reference, *plane_hit);
    const float angle_delta = wrapAngle(m_drag_current_angle - m_drag_start_angle);
    const Quat parent_q = parentWorldRotation(
        *scene, g_runtime_global_context.m_editor_selection->getSelection());
    entity->setRotation(applyRotationDrag(m_entity_rotation_at_drag_start, *m_active_axis,
                                          rot_axis, angle_delta, m_space, parent_q));
  } else if (m_mode == TransformGizmoMode::scale &&
             isScaleManipulator(*m_active_axis)) {
    const Ray ray = camera.makeRayFromWindowPosition(window_pos);
    float factor = 1.0f;
    if (*m_active_axis == ManipulatorAxis::trans_c) {
      const Plane view_plane =
          Plane::fromPointAndNormal(m_drag_basis.origin, m_drag_view_normal);
      const auto plane_hit = intersectRayPlane(ray, view_plane);
      if (!plane_hit) {
        return true;
      }
      const float r0 = std::max(m_scale_drag_start_param, 1e-4f);
      const float r1 = glm::length(*plane_hit - m_drag_basis.origin);
      factor = std::max(0.01f, r1 / r0);
    } else {
      glm::vec3 axis_dir = m_drag_basis.axis_x;
      if (*m_active_axis == ManipulatorAxis::trans_y) {
        axis_dir = m_drag_basis.axis_y;
      } else if (*m_active_axis == ManipulatorAxis::trans_z) {
        axis_dir = m_drag_basis.axis_z;
      }
      axis_dir = glm::normalize(axis_dir);
      const auto t = rayLineParameter(ray, m_drag_basis.origin, axis_dir);
      if (!t) {
        return true;
      }
      const glm::vec3 current_pt =
          closestPointOnLine(m_drag_basis.origin, axis_dir, ray.pointAt(*t));
      const float current_param = glm::dot(current_pt - m_drag_basis.origin, axis_dir);
      factor = std::max(0.01f, current_param / std::max(m_scale_drag_start_param, 1e-4f));
    }
    entity->setScale(
        applyScaleDrag(m_entity_scale_at_drag_start, *m_active_axis, factor));
  }

  markSceneDirty();
  syncInspectorLive();
  return true;
}

}  // namespace Blunder
