#include "runtime/function/render/gizmo/transform_gizmo_controller.h"

#include <SDL3/SDL.h>

#include "runtime/core/event/event.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/gizmo/transform_gizmo_pick.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"
#include "runtime/function/slint/slint_system.h"

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

void TransformGizmoController::setMode(const TransformGizmoMode mode,
                                       EditorCamera* camera) {
  if (m_mode == mode) {
    return;
  }
  cancelInteraction(camera);
  m_mode = mode;
}

void TransformGizmoController::toggleSpace() {
  m_space = (m_space == GizmoSpace::global) ? GizmoSpace::local : GizmoSpace::global;
}

void TransformGizmoController::cancelInteraction(EditorCamera* camera) {
  m_active_axis.reset();
  endCameraLock(camera);
}

bool TransformGizmoController::buildActiveGizmoBasis(GizmoBasis& out_basis) const {
  if (m_mode != TransformGizmoMode::translate &&
      m_mode != TransformGizmoMode::rotate) {
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
      setMode(TransformGizmoMode::translate, &camera);
      syncToolbar();
      return true;
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
      if (isDragging()) {
        if (Entity* entity = selectedEntity()) {
          if (isTranslationManipulator(*m_active_axis)) {
            entity->setPosition(m_entity_position_at_drag_start);
          } else if (isRotationManipulator(*m_active_axis)) {
            entity->setRotation(m_entity_rotation_at_drag_start);
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
  if (mouse.getMouseButton() != SDL_BUTTON_LEFT || !mouse.hasMousePosition()) {
    return false;
  }
  if (m_mode != TransformGizmoMode::translate &&
      m_mode != TransformGizmoMode::rotate) {
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
  const float scale = computeHandleUniformScale(
      camera.getPosition(), basis.origin, camera.getViewportHeight(),
      camera.getVerticalFov(),
      camera.getProjectionMode() == EditorCamera::ProjectionMode::perspective,
      camera.getOrthoSize());
  m_handle_scale = scale;
  m_drag_view_normal =
      glm::normalize(basis.origin - glm::vec3(camera.getPosition()));

  TransformGizmoPickContext pick_ctx{};
  pick_ctx.ray = ray;
  pick_ctx.basis = basis;
  pick_ctx.uniform_scale = scale;
  pick_ctx.camera_forward = camera.getForwardDirection();
  pick_ctx.camera_position = camera.getPosition();

  std::optional<ManipulatorAxis> hit;
  if (m_mode == TransformGizmoMode::translate) {
    hit = pickTranslationGizmoHandle(pick_ctx);
  } else {
    hit = pickRotationGizmoHandle(pick_ctx);
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

  if (isTranslationManipulator(*hit)) {
    const auto start_pt = dragWorldPoint(*hit, ray, basis);
    m_drag_start_world = start_pt.value_or(basis.origin);
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

  m_active_axis.reset();
  endCameraLock(&camera);
  markSceneDirty();
  return true;
}

bool TransformGizmoController::onMouseMoved(Event& event, EditorCamera& camera) {
  if (!m_active_axis) {
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

  const Ray ray = camera.makeRayFromWindowPosition(window_pos);

  if (isTranslationManipulator(*m_active_axis)) {
    const auto current_pt = dragWorldPoint(*m_active_axis, ray, m_drag_basis);
    if (!current_pt) {
      return true;
    }
    const glm::vec3 delta = worldTranslationDelta(
        m_drag_basis, *m_active_axis, m_drag_start_world, *current_pt);
    entity->setPosition(m_entity_position_at_drag_start + Vec3(delta));
  } else if (isRotationManipulator(*m_active_axis)) {
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
  }

  markSceneDirty();
  syncInspectorLive();
  return true;
}

std::optional<glm::vec3> TransformGizmoController::dragWorldPoint(
    const ManipulatorAxis axis, const Ray& ray, const GizmoBasis& basis) const {
  const float plane_half =
      m_handle_scale * TransformGizmoMetrics::k_plane_half_extent_factor /
      TransformGizmoMetrics::k_axis_length_factor;

  if (axis == ManipulatorAxis::trans_x || axis == ManipulatorAxis::trans_y ||
      axis == ManipulatorAxis::trans_z) {
    glm::vec3 dir = basis.axis_x;
    if (axis == ManipulatorAxis::trans_y) {
      dir = basis.axis_y;
    } else if (axis == ManipulatorAxis::trans_z) {
      dir = basis.axis_z;
    }
    const auto t = rayLineParameter(ray, basis.origin, dir);
    if (!t) {
      return std::nullopt;
    }
    return closestPointOnLine(basis.origin, dir, ray.pointAt(*t));
  }

  if (axis == ManipulatorAxis::trans_xy) {
    return intersectRayPlaneQuad(ray, basis.origin, basis.axis_x, basis.axis_y,
                                 plane_half);
  }
  if (axis == ManipulatorAxis::trans_yz) {
    return intersectRayPlaneQuad(ray, basis.origin, basis.axis_y, basis.axis_z,
                                 plane_half);
  }
  if (axis == ManipulatorAxis::trans_zx) {
    return intersectRayPlaneQuad(ray, basis.origin, basis.axis_z, basis.axis_x,
                                 plane_half);
  }

  const Plane view_plane =
      Plane::fromPointAndNormal(basis.origin, m_drag_view_normal);
  return intersectRayPlane(ray, view_plane);
}

}  // namespace Blunder
