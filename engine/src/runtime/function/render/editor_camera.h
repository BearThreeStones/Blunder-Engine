#pragma once

#include <cstdint>

#include "runtime/core/event/event.h"
#include "runtime/core/math/geometry.h"
#include "runtime/core/math/math_types.h"

namespace Blunder {

class MouseButtonPressedEvent;
class MouseButtonReleasedEvent;
class MouseMovedEvent;
class MouseScrolledEvent;
class KeyPressedEvent;
class WindowLostFocusEvent;
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

  bool isWindowPositionInViewport(const Vec2& window_position) const;
  Vec2 windowToViewportLocal(const Vec2& window_position) const;
  Vec2 viewportLocalToNdc(const Vec2& viewport_position) const;
  Ray makeRayFromWindowPosition(const Vec2& window_position) const;

  void setViewportRect(int32_t x, int32_t y, float width, float height);
  void setViewportSize(float width, float height);
  void setProjectionMode(ProjectionMode mode);

 private:
  enum class InteractionMode : uint32_t {
    none = 0,
    pan,
    free_look,
  };

  bool onMouseButtonPressed(MouseButtonPressedEvent& event);
  bool onMouseButtonReleased(MouseButtonReleasedEvent& event);
  bool onMouseMoved(MouseMovedEvent& event);
  bool onMouseScrolled(MouseScrolledEvent& event);
  bool onKeyPressed(KeyPressedEvent& event);
  bool onWindowLostFocus(WindowLostFocusEvent& event);

  void beginFreeLook();
  void endFreeLook();
  Vec2 getCurrentCursorWindowPosition() const;
  bool isViewportReady() const;
  bool isCursorInViewport() const;
  void applyFreeLookRotation(const Vec2& mouse_delta);
  void updateDirectionVectors();
  void updateFreeLook(float delta_time, const bool* keyboard_state);
  void pan();
  void zoom();

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
  int32_t m_viewport_origin_x{0};
  int32_t m_viewport_origin_y{0};
  float m_vertical_fov{glm::radians(45.0f)};
  float m_near_clip{0.1f};
  float m_far_clip{1000.0f};
  float m_ortho_size{10.0f};
  ProjectionMode m_projection_mode{ProjectionMode::perspective};
  InteractionMode m_interaction_mode{InteractionMode::none};
  bool m_right_drag_started_in_viewport{false};
  bool m_middle_drag_started_in_viewport{false};

  Vec2 m_mouse_delta_accumulator{0.0f, 0.0f};
  float m_scroll_delta_accumulator{0.0f};
  Vec2 m_last_mouse_position{0.0f, 0.0f};
  bool m_has_last_mouse_position{false};
};

}  // namespace Blunder
