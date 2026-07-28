#include "runtime/function/render/overlay/camera_gizmo_controller.h"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "runtime/core/event/event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/core/math/geometry.h"
#include "runtime/function/editor/document_history_helpers.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/editor_scene_edit_system.h"
#include "runtime/function/editor/editor_selection_system.h"
#include "runtime/function/editor/viewport_pick_system.h"
#include "runtime/function/editor/viewport_pick_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/overlay/camera_gizmo_geometry.h"
#include "runtime/function/render/overlay/camera_gizmo_math.h"
#include "runtime/function/render/render_system.h"
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

EntityId soleSelectedCameraEntity(SceneInstance* scene) {
  if (!g_runtime_global_context.m_editor_selection || scene == nullptr) {
    return k_invalid_entity_id;
  }
  const eastl::vector<EntityId> ids =
      g_runtime_global_context.m_editor_selection->getSelectedIds();
  if (ids.size() != 1) {
    return k_invalid_entity_id;
  }
  if (scene->getCamera(ids[0]) == nullptr) {
    return k_invalid_entity_id;
  }
  return ids[0];
}

void markSceneDirty() {
  if (g_runtime_global_context.m_editor_scene_edit) {
    g_runtime_global_context.m_editor_scene_edit->markDirty();
  }
}

}  // namespace

void CameraGizmoController::endCameraLock(EditorCamera* camera) {
  if (camera != nullptr) {
    camera->setInteractionLocked(false);
  }
}

void CameraGizmoController::requestViewportRedraw() const {
  if (g_runtime_global_context.m_render_system) {
    g_runtime_global_context.m_render_system->requestViewportRedraw();
  }
}

void CameraGizmoController::syncInspectorLive() const {
  if (g_runtime_global_context.m_slint_system) {
    g_runtime_global_context.m_slint_system->syncInspectorFromSelection();
  }
}

void CameraGizmoController::onEvent(Event& event, EditorCamera& camera) {
  switch (event.getEventType()) {
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

bool CameraGizmoController::onMousePressed(Event& event, EditorCamera& camera) {
  auto& mouse = static_cast<MouseButtonPressedEvent&>(event);
  if (mouse.getMouseButton() != SDL_BUTTON_LEFT || !mouse.hasMousePosition()) {
    return false;
  }

  const Vec2 window_pos(mouse.getX(), mouse.getY());
  if (!camera.isWindowPositionInViewport(window_pos)) {
    return false;
  }

  SceneInstance* scene = activeScene();
  const EntityId entity_id = soleSelectedCameraEntity(scene);
  if (!isValid(entity_id) || scene == nullptr) {
    return false;
  }

  const CameraComponent* camera_comp = scene->getCamera(entity_id);
  if (camera_comp == nullptr) {
    return false;
  }

  const Vec2 viewport_local = camera.windowToViewportLocal(window_pos);
  const glm::vec2 pointer(viewport_local.x, viewport_local.y);
  const float vp_w = camera.getViewportWidth();
  const float vp_h = std::max(camera.getViewportHeight(), 1.0f);
  const float aspect = vp_w / vp_h;
  const glm::mat4 view = camera.getViewMatrix();
  const glm::mat4 proj = camera.getProjectionMatrix();
  const glm::mat4 world = scene->getWorldMatrix(entity_id);

  const float fov_rad = glm::radians(camera_comp->vertical_fov_degrees);
  const CameraGizmoFrame frame =
      buildCameraGizmoFrameLocal(fov_rad, aspect, kCameraGizmoDisplayDistance);

  const std::optional<CameraGizmoHandleKind> handle =
      hitTestCameraGizmoHandlesViewportLocal(pointer, frame, *camera_comp, world,
                                             view, proj, vp_w, vp_h);
  if (!handle.has_value() ||
      *handle == CameraGizmoHandleKind::none) {
    return false;
  }

  m_active_handle = *handle;
  m_entity_id = entity_id;
  m_camera_at_drag_start = *camera_comp;

  if (g_runtime_global_context.m_viewport_pick) {
    g_runtime_global_context.m_viewport_pick->suppressNextLeftReleasePick();
  }
  camera.setInteractionLocked(true);
  requestViewportRedraw();
  return true;
}

bool CameraGizmoController::onMouseReleased(Event& event, EditorCamera& camera) {
  auto& mouse = static_cast<MouseButtonReleasedEvent&>(event);
  if (mouse.getMouseButton() != SDL_BUTTON_LEFT ||
      m_active_handle == CameraGizmoHandleKind::none) {
    return false;
  }

  SceneInstance* scene = activeScene();
  const CameraComponent* after_camera =
      scene != nullptr ? scene->getCamera(m_entity_id) : nullptr;

  if (scene != nullptr && after_camera != nullptr && isValid(m_entity_id)) {
    const bool changed =
        m_camera_at_drag_start.vertical_fov_degrees !=
            after_camera->vertical_fov_degrees ||
        m_camera_at_drag_start.near_clip != after_camera->near_clip ||
        m_camera_at_drag_start.far_clip != after_camera->far_clip ||
        m_camera_at_drag_start.is_main != after_camera->is_main;
    if (changed) {
      pushDocumentCommand(makeSetCameraComponentCommand(
          scene, m_entity_id, m_camera_at_drag_start, *after_camera,
          SelectionSnapshot{m_entity_id}, currentSelectionSnapshot()));
    }
  }

  m_active_handle = CameraGizmoHandleKind::none;
  m_entity_id = k_invalid_entity_id;
  endCameraLock(&camera);
  markSceneDirty();
  requestViewportRedraw();
  return true;
}

bool CameraGizmoController::onMouseMoved(Event& event, EditorCamera& camera) {
  if (m_active_handle == CameraGizmoHandleKind::none) {
    return false;
  }

  auto& mouse = static_cast<MouseMovedEvent&>(event);
  const Vec2 window_pos(mouse.getX(), mouse.getY());
  if (!camera.isWindowPositionInViewport(window_pos)) {
    return true;
  }

  if (m_active_handle == CameraGizmoHandleKind::fov_top) {
    applyFovDrag(camera, window_pos);
  } else if (m_active_handle == CameraGizmoHandleKind::near_clip) {
    applyClipDrag(camera, window_pos, true);
  } else if (m_active_handle == CameraGizmoHandleKind::far_clip) {
    applyClipDrag(camera, window_pos, false);
  }

  markSceneDirty();
  syncInspectorLive();
  requestViewportRedraw();
  return true;
}

void CameraGizmoController::applyFovDrag(EditorCamera& camera,
                                       const Vec2& window_pos) {
  SceneInstance* scene = activeScene();
  if (scene == nullptr || !isValid(m_entity_id)) {
    return;
  }
  const CameraComponent* existing = scene->getCamera(m_entity_id);
  if (existing == nullptr) {
    return;
  }

  const glm::mat4 world = scene->getWorldMatrix(m_entity_id);
  const glm::vec3 look_world =
      glm::normalize(glm::vec3(world * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
  const glm::vec3 plane_point =
      glm::vec3(world * glm::vec4(0.0f, 0.0f, -kCameraGizmoDisplayDistance, 1.0f));
  const Plane plane = Plane::fromPointAndNormal(plane_point, look_world);

  const Ray ray = camera.makeRayFromWindowPosition(window_pos);
  const std::optional<glm::vec3> hit = intersectRayPlane(ray, plane);
  if (!hit.has_value()) {
    return;
  }

  const glm::mat4 inv_world = glm::inverse(world);
  const glm::vec3 local = glm::vec3(inv_world * glm::vec4(*hit, 1.0f));
  const float half_h = std::abs(local.y);
  const float new_fov =
      verticalFovDegreesFromHalfHeight(half_h, kCameraGizmoDisplayDistance);

  CameraComponent updated = *existing;
  updated.vertical_fov_degrees = new_fov;
  scene->setCamera(m_entity_id, updated);
}

void CameraGizmoController::applyClipDrag(EditorCamera& camera,
                                          const Vec2& window_pos, bool near_clip) {
  SceneInstance* scene = activeScene();
  if (scene == nullptr || !isValid(m_entity_id)) {
    return;
  }
  const CameraComponent* existing = scene->getCamera(m_entity_id);
  if (existing == nullptr) {
    return;
  }

  const glm::mat4 world = scene->getWorldMatrix(m_entity_id);
  const glm::vec3 origin_world = glm::vec3(world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
  const glm::vec3 look_world =
      glm::normalize(glm::vec3(world * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));

  const Ray ray = camera.makeRayFromWindowPosition(window_pos);
  const std::optional<float> t = rayLineParameter(ray, origin_world, look_world);
  if (!t.has_value()) {
    return;
  }

  const float distance = std::max(*t, kCameraClipMinDistance);
  CameraComponent updated = *existing;
  if (near_clip) {
    setCameraNearClip(updated.near_clip, updated.far_clip, distance);
  } else {
    setCameraFarClip(updated.near_clip, updated.far_clip, distance);
  }
  scene->setCamera(m_entity_id, updated);
}

}  // namespace Blunder
