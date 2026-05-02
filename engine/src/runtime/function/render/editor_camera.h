#pragma once

#include "runtime/core/event/event.h"
#include "runtime/core/math/math_types.h"

namespace Blunder {

class MouseMovedEvent;
class MouseScrolledEvent;
class WindowSystem;

class EditorCamera final {
 public:
  explicit EditorCamera(WindowSystem* window_system);

  void onUpdate(float delta_time);
  void onEvent(Event& event);

  const Mat4& getViewMatrix() const { return m_view_matrix; }
  const Mat4& getProjectionMatrix() const { return m_projection_matrix; }
  const Vec3& getPosition() const { return m_position; }
  const Vec3& getForwardDirection() const { return m_forward_direction; }
  const Vec3& getRightDirection() const { return m_right_direction; }
  const Vec3& getUpDirection() const { return m_up_direction; }
  const Vec3& getFocalPoint() const { return m_focal_point; }

  void setViewportSize(float width, float height);

 private:
  bool onMouseMoved(MouseMovedEvent& event);
  bool onMouseScrolled(MouseScrolledEvent& event);

  void orbit(float delta_time);
  void pan(float delta_time);
  void zoom(float delta_time);

  void updateViewMatrix();
  void updateProjectionMatrix();

  WindowSystem* m_window_system{nullptr};

  // Core state (actively modified)
  Vec3 m_focal_point{0.0f, 0.0f, 0.0f};
  float m_distance{6.0f};
  float m_pitch{glm::radians(20.0f)};
  float m_yaw{glm::radians(45.0f)};

  // Derived state (calculated every frame)
  Vec3 m_position{0.0f, 0.0f, 0.0f};
  Vec3 m_forward_direction{0.0f, 0.0f, -1.0f};
  Vec3 m_right_direction{1.0f, 0.0f, 0.0f};
  Vec3 m_up_direction{0.0f, 0.0f, 1.0f};
  Mat4 m_view_matrix{1.0f};
  Mat4 m_projection_matrix{1.0f};

  float m_viewport_width{1280.0f};
  float m_viewport_height{720.0f};
  float m_vertical_fov{glm::radians(45.0f)};
  float m_near_clip{0.1f};
  float m_far_clip{1000.0f};

  Vec2 m_mouse_delta_accumulator{0.0f, 0.0f};
  float m_scroll_delta_accumulator{0.0f};
  Vec2 m_last_mouse_position{0.0f, 0.0f};
  bool m_has_last_mouse_position{false};
};

}  // namespace Blunder
