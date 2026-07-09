#pragma once

#include <optional>

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/gizmo/translate_modal_session.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"

namespace Blunder {

class EditorCamera;
class Entity;
class Event;

class TransformGizmoController final {
 public:
  TransformGizmoMode getMode() const { return m_mode; }
  GizmoSpace getSpace() const { return m_space; }
  bool isDragging() const {
    return m_active_axis.has_value() || m_translate_session.isActive();
  }
  bool wantsCameraLock() const { return isDragging(); }
  ManipulatorAxis getActiveAxis() const {
    if (m_active_axis.has_value()) {
      return *m_active_axis;
    }
    if (m_translate_session.isActive()) {
      return m_translate_session.activeHandle();
    }
    return ManipulatorAxis::trans_x;
  }

  float getRotationDragStartAngle() const { return m_drag_start_angle; }
  float getRotationDragCurrentAngle() const { return m_drag_current_angle; }
  float getRotationArcMeshBase() const { return m_rotation_arc_mesh_base; }
  bool isTranslateModalSessionActive() const {
    return m_translate_session.isActive();
  }
  const TranslateModalSession& translateModalSession() const {
    return m_translate_session;
  }

  void setMode(TransformGizmoMode mode, EditorCamera* camera = nullptr);
  void toggleSpace();
  void cancelInteraction(EditorCamera* camera = nullptr);

  void onEvent(Event& event, EditorCamera& camera);

  bool buildActiveGizmoBasis(GizmoBasis& out_basis) const;

 private:
  void endCameraLock(EditorCamera* camera);
  void setTranslateModalCursor();
  void clearTranslateModalCursor();
  void requestViewportRedraw() const;

  bool onKeyPressed(Event& event, EditorCamera& camera);
  bool onMousePressed(Event& event, EditorCamera& camera);
  bool onMouseReleased(Event& event, EditorCamera& camera);
  bool onMouseMoved(Event& event, EditorCamera& camera);

  void restoreEntityTransformAtDragStart(Entity& entity) const;
  bool beginGrabFromSelection(EditorCamera& camera);
  bool confirmTranslateModalSession(EditorCamera& camera);
  bool cancelTranslateModalSession(EditorCamera& camera);

  TransformGizmoMode m_mode{TransformGizmoMode::none};
  GizmoSpace m_space{GizmoSpace::global};
  std::optional<ManipulatorAxis> m_active_axis;

  glm::vec3 m_drag_start_world{0.0f};
  Vec3 m_entity_position_at_drag_start{0.0f};
  Quat m_entity_rotation_at_drag_start{glm::identity<Quat>()};
  Vec3 m_entity_scale_at_drag_start{1.0f};
  float m_scale_drag_start_param{1.0f};
  glm::vec3 m_rotation_drag_reference{1.0f, 0.0f, 0.0f};
  float m_drag_start_angle{0.0f};
  float m_drag_current_angle{0.0f};
  float m_rotation_arc_mesh_base{0.0f};

  GizmoBasis m_drag_basis{};
  TranslateModalSession m_translate_session{};
  float m_gizmo_group_scale{1.0f};
  glm::vec3 m_drag_view_normal{0.0f, 0.0f, 1.0f};
  EditorCamera* m_locked_camera{nullptr};
};

}  // namespace Blunder
