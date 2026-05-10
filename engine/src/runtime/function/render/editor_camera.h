#pragma once

#include <cstdint>

#include "runtime/core/event/event.h"
#include "runtime/core/math/math_types.h"

namespace Blunder {

class MouseMovedEvent;
class MouseScrolledEvent;
class WindowSystem;

class EditorCamera final {
 public:
  enum class ProjectionMode : uint32_t {
    perspective = 0,
    orthographic = 1,
  };

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
  ProjectionMode getProjectionMode() const { return m_projection_mode; }
  float getDistance() const { return m_distance; }
  float getVerticalFov() const { return m_vertical_fov; }
  float getNearClip() const { return m_near_clip; }
  float getFarClip() const { return m_far_clip; }
  float getOrthoSize() const { return m_ortho_size; }

  void setViewportSize(float width, float height);
  void setProjectionMode(ProjectionMode mode);

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
  float m_ortho_size{10.0f};
  ProjectionMode m_projection_mode{ProjectionMode::perspective};

  Vec2 m_mouse_delta_accumulator{0.0f, 0.0f};
  float m_scroll_delta_accumulator{0.0f};
  Vec2 m_last_mouse_position{0.0f, 0.0f};
  bool m_has_last_mouse_position{false};
};

}  // namespace Blunder
