#include "runtime/function/render/editor_camera.h"

#include <algorithm>

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/core/base/macro.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

namespace {

constexpr float k_pan_speed = 2.5f;
constexpr float k_orbit_speed = 0.015f;
constexpr float k_zoom_speed = 12.0f;
constexpr float k_min_distance = 0.1f;
constexpr float k_max_pitch = glm::radians(89.0f);
constexpr float k_min_pitch = glm::radians(-89.0f);

}  // namespace

EditorCamera::EditorCamera(WindowSystem* window_system)
    : m_window_system(window_system) {
  updateProjectionMatrix();
  updateViewMatrix();
}

void EditorCamera::onUpdate(float delta_time) {
  const bool* keyboard_state = SDL_GetKeyboardState(nullptr);
  const bool is_alt_pressed = keyboard_state &&
                              (keyboard_state[SDL_SCANCODE_LALT] ||
                               keyboard_state[SDL_SCANCODE_RALT]);
  if (keyboard_state) {
    if (keyboard_state[SDL_SCANCODE_P]) {
      setProjectionMode(ProjectionMode::perspective);
    } else if (keyboard_state[SDL_SCANCODE_O]) {
      setProjectionMode(ProjectionMode::orthographic);
    }
  }

  if (is_alt_pressed && m_window_system) {
    if (m_window_system->isMouseButtonDown(SDL_BUTTON_LEFT)) {
      orbit(delta_time);
    } else if (m_window_system->isMouseButtonDown(SDL_BUTTON_MIDDLE)) {
      pan(delta_time);
    } else {
      m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
    }
  } else {
    m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
  }

  zoom(delta_time);
  updateViewMatrix();
}

void EditorCamera::onEvent(Event& event) {
  EventDispatcher dispatcher(event);
  dispatcher.dispatch<MouseMovedEvent>(
      [this](MouseMovedEvent& e) { return onMouseMoved(e); });
  dispatcher.dispatch<MouseScrolledEvent>(
      [this](MouseScrolledEvent& e) { return onMouseScrolled(e); });
}

bool EditorCamera::onMouseMoved(MouseMovedEvent& event) {
  const Vec2 current_mouse_position(event.getX(), event.getY());
  if (!m_has_last_mouse_position) {
    m_last_mouse_position = current_mouse_position;
    m_has_last_mouse_position = true;
    return false;
  }

  const Vec2 delta = current_mouse_position - m_last_mouse_position;
  m_mouse_delta_accumulator += delta;
  m_last_mouse_position = current_mouse_position;
  return false;
}

bool EditorCamera::onMouseScrolled(MouseScrolledEvent& event) {
  m_scroll_delta_accumulator += event.getYOffset();
  return false;
}

void EditorCamera::setViewportSize(float width, float height) {
  if (width <= 0.0f || height <= 0.0f) {
    return;
  }

  m_viewport_width = width;
  m_viewport_height = height;
  updateProjectionMatrix();
}

void EditorCamera::setProjectionMode(ProjectionMode mode) {
  if (m_projection_mode == mode) {
    return;
  }
  m_projection_mode = mode;
  updateProjectionMatrix();
  LOG_INFO("[EditorCamera] projection mode switched to {}",
           m_projection_mode == ProjectionMode::perspective ? "perspective"
                                                            : "orthographic");
}

void EditorCamera::orbit(float delta_time) {
  const Vec2 frame_delta = m_mouse_delta_accumulator * k_orbit_speed * delta_time;
  m_yaw += frame_delta.x;
  m_pitch -= frame_delta.y;
  m_pitch = std::clamp(m_pitch, k_min_pitch, k_max_pitch);
  LOG_DEBUG("[EditorCamera] orbit yaw={}, pitch={}, distance={}", m_yaw, m_pitch,
            m_distance);
  m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
}

void EditorCamera::pan(float delta_time) {
  const float distance_factor = std::max(m_distance, 1.0f);
  const float pan_scale = k_pan_speed * distance_factor * 0.01f;
  const float pan_x = -m_mouse_delta_accumulator.x * pan_scale * delta_time;
  const float pan_y = m_mouse_delta_accumulator.y * pan_scale * delta_time;
  m_focal_point += m_right_direction * pan_x;
  m_focal_point += m_up_direction * pan_y;
  LOG_DEBUG("[EditorCamera] pan focal=({}, {}, {})", m_focal_point.x,
            m_focal_point.y, m_focal_point.z);
  m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
}

void EditorCamera::zoom(float delta_time) {
  if (m_scroll_delta_accumulator == 0.0f) {
    return;
  }

  m_distance -= m_scroll_delta_accumulator * k_zoom_speed * delta_time;
  m_distance = std::max(m_distance, k_min_distance);
  LOG_DEBUG("[EditorCamera] zoom distance={}", m_distance);
  m_scroll_delta_accumulator = 0.0f;
}

void EditorCamera::updateViewMatrix() {
  const Vec3 world_up(0.0f, 0.0f, 1.0f);
  m_forward_direction.x = cos(m_pitch) * cos(m_yaw);
  m_forward_direction.y = cos(m_pitch) * sin(m_yaw);
  m_forward_direction.z = sin(m_pitch);
  m_forward_direction = glm::normalize(m_forward_direction);

  m_right_direction = glm::normalize(glm::cross(m_forward_direction, world_up));
  m_up_direction = glm::normalize(glm::cross(m_right_direction, m_forward_direction));

  m_position = m_focal_point - m_forward_direction * m_distance;
  m_view_matrix = glm::lookAt(m_position, m_focal_point, m_up_direction);
}

void EditorCamera::updateProjectionMatrix() {
  const float aspect = m_viewport_width / m_viewport_height;
  if (m_projection_mode == ProjectionMode::perspective) {
    m_projection_matrix =
        glm::perspective(m_vertical_fov, aspect, m_near_clip, m_far_clip);
  } else {
    const float ortho_half_h = m_ortho_size * 0.5f;
    const float ortho_half_w = ortho_half_h * aspect;
    m_projection_matrix =
        glm::ortho(-ortho_half_w, ortho_half_w, -ortho_half_h, ortho_half_h,
                   m_near_clip, m_far_clip);
  }
  // Vulkan NDC has inverted Y and Z range [0, 1].
  m_projection_matrix[1][1] *= -1.0f;
}

}  // namespace Blunder
