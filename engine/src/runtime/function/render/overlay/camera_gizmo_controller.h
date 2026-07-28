#pragma once

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/overlay/camera_gizmo_hit_test.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class EditorCamera;
class Event;

class CameraGizmoController final {
 public:
  bool isDragging() const { return m_active_handle != CameraGizmoHandleKind::none; }

  void onEvent(Event& event, EditorCamera& camera);

 private:
  void endCameraLock(EditorCamera* camera);
  void requestViewportRedraw() const;
  void syncInspectorLive() const;

  bool onMousePressed(Event& event, EditorCamera& camera);
  bool onMouseReleased(Event& event, EditorCamera& camera);
  bool onMouseMoved(Event& event, EditorCamera& camera);

  void applyFovDrag(EditorCamera& camera, const Vec2& window_pos);
  void applyClipDrag(EditorCamera& camera, const Vec2& window_pos, bool near_clip);

  CameraGizmoHandleKind m_active_handle{CameraGizmoHandleKind::none};
  EntityId m_entity_id{k_invalid_entity_id};
  CameraComponent m_camera_at_drag_start{};
};

}  // namespace Blunder
