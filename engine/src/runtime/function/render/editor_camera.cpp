#include "runtime/function/render/editor_camera.h"

#include <algorithm>

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/core/event/application_event.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

namespace {

constexpr float k_pan_speed = 0.0015f;
constexpr float k_free_look_rotate_speed = 0.0025f;
constexpr float k_free_look_move_speed = 6.0f;
constexpr float k_free_look_sprint_multiplier = 3.0f;
constexpr float k_dolly_speed = 1.2f;
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
  if (keyboard_state) {
    if (keyboard_state[SDL_SCANCODE_P]) {
      setProjectionMode(ProjectionMode::perspective);
    } else if (keyboard_state[SDL_SCANCODE_O]) {
      setProjectionMode(ProjectionMode::orthographic);
    }
  }

  if (m_window_system) {
    const bool right_mouse_down =
        m_window_system->isMouseButtonDown(SDL_BUTTON_RIGHT);
    const bool middle_mouse_down =
        m_window_system->isMouseButtonDown(SDL_BUTTON_MIDDLE);

    if (!right_mouse_down) {
      m_right_drag_started_in_viewport = false;
    }

    if (!middle_mouse_down) {
      m_middle_drag_started_in_viewport = false;
    }

    InteractionMode desired_mode = InteractionMode::none;
    if (right_mouse_down &&
        m_right_drag_started_in_viewport) {
      desired_mode = InteractionMode::free_look;
    } else if (middle_mouse_down && m_middle_drag_started_in_viewport) {
      desired_mode = InteractionMode::pan;
    }

    if (desired_mode != m_interaction_mode) {
      if (m_interaction_mode == InteractionMode::free_look) {
        endFreeLook();
      }

      m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
      m_has_last_mouse_position = false;

      if (desired_mode == InteractionMode::free_look) {
        beginFreeLook();
      }

      m_interaction_mode = desired_mode;
    }

    if (m_interaction_mode == InteractionMode::free_look) {
      updateFreeLook(delta_time, keyboard_state);
    } else if (m_interaction_mode == InteractionMode::pan) {
      pan();
    }
  }

  zoom();
  updateViewMatrix();
}

void EditorCamera::onEvent(Event& event) {
  EventDispatcher dispatcher(event);
  dispatcher.dispatch<MouseButtonPressedEvent>(
    [this](MouseButtonPressedEvent& e) { return onMouseButtonPressed(e); });
  dispatcher.dispatch<MouseButtonReleasedEvent>(
    [this](MouseButtonReleasedEvent& e) { return onMouseButtonReleased(e); });
  dispatcher.dispatch<MouseMovedEvent>(
      [this](MouseMovedEvent& e) { return onMouseMoved(e); });
  dispatcher.dispatch<MouseScrolledEvent>(
      [this](MouseScrolledEvent& e) { return onMouseScrolled(e); });
  dispatcher.dispatch<KeyPressedEvent>(
      [this](KeyPressedEvent& e) { return onKeyPressed(e); });
  dispatcher.dispatch<WindowLostFocusEvent>(
      [this](WindowLostFocusEvent& e) { return onWindowLostFocus(e); });
}

bool EditorCamera::onMouseButtonPressed(MouseButtonPressedEvent& event) {
  const Vec2 mouse_position =
      event.hasMousePosition() ? Vec2(event.getX(), event.getY())
                               : getCurrentCursorWindowPosition();
  const bool in_viewport = isWindowPositionInViewport(mouse_position);

  if (event.getMouseButton() == SDL_BUTTON_RIGHT) {
    m_right_drag_started_in_viewport = in_viewport;
  } else if (event.getMouseButton() == SDL_BUTTON_MIDDLE) {
    m_middle_drag_started_in_viewport = in_viewport;
    m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
    m_last_mouse_position = mouse_position;
    m_has_last_mouse_position = in_viewport;
  }

  return false;
}

bool EditorCamera::onMouseButtonReleased(MouseButtonReleasedEvent& event) {
  if (event.getMouseButton() == SDL_BUTTON_RIGHT) {
    m_right_drag_started_in_viewport = false;
  } else if (event.getMouseButton() == SDL_BUTTON_MIDDLE) {
    m_middle_drag_started_in_viewport = false;
    m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
    m_has_last_mouse_position = false;
  }

  return false;
}

bool EditorCamera::onMouseMoved(MouseMovedEvent& event) {
  const Vec2 current_mouse_position(event.getX(), event.getY());
  const bool should_free_look =
      m_interaction_mode == InteractionMode::free_look ||
      (m_window_system != nullptr && m_right_drag_started_in_viewport &&
       m_window_system->isMouseButtonDown(SDL_BUTTON_RIGHT));
  if (should_free_look) {
    applyFreeLookRotation(Vec2(event.getDeltaX(), event.getDeltaY()));
    return false;
  }

  const bool should_track_mouse =
      m_interaction_mode == InteractionMode::pan ||
      m_middle_drag_started_in_viewport ||
      (m_interaction_mode == InteractionMode::none &&
       isWindowPositionInViewport(current_mouse_position));
  if (!should_track_mouse) {
    m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
    m_has_last_mouse_position = false;
    return false;
  }

  if (!m_has_last_mouse_position) {
    m_last_mouse_position = current_mouse_position;
    m_has_last_mouse_position = true;
    return false;
  }

  const Vec2 delta = current_mouse_position - m_last_mouse_position;
  if (m_interaction_mode == InteractionMode::pan ||
      m_middle_drag_started_in_viewport) {
    m_mouse_delta_accumulator += delta;
  }
  m_last_mouse_position = current_mouse_position;
  return false;
}

bool EditorCamera::onMouseScrolled(MouseScrolledEvent& event) {
  const Vec2 mouse_position =
      event.hasMousePosition() ? Vec2(event.getMouseX(), event.getMouseY())
                               : getCurrentCursorWindowPosition();
  if (!isWindowPositionInViewport(mouse_position)) {
    return false;
  }

  m_scroll_delta_accumulator += event.getYOffset();
  return false;
}

bool EditorCamera::onKeyPressed(KeyPressedEvent& event) {
  if (event.isRepeat()) {
    return false;
  }

  if (event.getKeyCode() == SDLK_F) {
    LOG_INFO("[EditorCamera] focus action requested, but scene focus target is not implemented yet");
  }

  return false;
}

bool EditorCamera::onWindowLostFocus(WindowLostFocusEvent& event) {
  (void)event;

  if (m_interaction_mode == InteractionMode::free_look) {
    endFreeLook();
  }

  m_interaction_mode = InteractionMode::none;
  m_right_drag_started_in_viewport = false;
  m_middle_drag_started_in_viewport = false;
  m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
  m_scroll_delta_accumulator = 0.0f;
  m_has_last_mouse_position = false;
  return false;
}

bool EditorCamera::isWindowPositionInViewport(
    const Vec2& window_position) const {
  if (!m_window_system || !isViewportReady()) {
    return true;
  }

  const float min_x = static_cast<float>(m_viewport_origin_x);
  const float min_y = static_cast<float>(m_viewport_origin_y);
  const float max_x = min_x + m_viewport_width;
  const float max_y = min_y + m_viewport_height;
  return window_position.x >= min_x && window_position.x <= max_x &&
         window_position.y >= min_y && window_position.y <= max_y;
}

Vec2 EditorCamera::windowToViewportLocal(const Vec2& window_position) const {
  if (!isViewportReady()) {
    return window_position;
  }

  return Vec2(window_position.x - static_cast<float>(m_viewport_origin_x),
              window_position.y - static_cast<float>(m_viewport_origin_y));
}

Vec2 EditorCamera::viewportLocalToNdc(const Vec2& viewport_position) const {
  if (!isViewportReady()) {
    return Vec2(0.0f, 0.0f);
  }

  const float normalized_x = viewport_position.x / m_viewport_width;
  const float normalized_y = viewport_position.y / m_viewport_height;
  return Vec2(normalized_x * 2.0f - 1.0f, 1.0f - normalized_y * 2.0f);
}

Ray EditorCamera::makeRayFromWindowPosition(const Vec2& window_position) const {
  const Vec2 viewport_position = windowToViewportLocal(window_position);
  const Vec2 ndc = viewportLocalToNdc(viewport_position);
  const Mat4 inverse_view_projection =
      glm::inverse(m_projection_matrix * m_view_matrix);

  Vec4 near_world = inverse_view_projection * Vec4(ndc.x, ndc.y, 0.0f, 1.0f);
  Vec4 far_world = inverse_view_projection * Vec4(ndc.x, ndc.y, 1.0f, 1.0f);
  near_world /= near_world.w;
  far_world /= far_world.w;

  const Vec3 near_point(near_world.x, near_world.y, near_world.z);
  const Vec3 far_point(far_world.x, far_world.y, far_world.z);
  const Vec3 direction = glm::normalize(far_point - near_point);

  if (m_projection_mode == ProjectionMode::orthographic) {
    return Ray{near_point, direction};
  }

  return Ray{m_position, direction};
}

void EditorCamera::setViewportRect(int32_t x, int32_t y, float width,
                                   float height) {
  m_viewport_origin_x = x;
  m_viewport_origin_y = y;
  setViewportSize(width, height);
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

void EditorCamera::beginFreeLook() {
  if (!m_window_system) {
    return;
  }

  m_window_system->setFocusMode(true);
  float relative_x = 0.0f;
  float relative_y = 0.0f;
  SDL_GetRelativeMouseState(&relative_x, &relative_y);
}

void EditorCamera::endFreeLook() {
  if (!m_window_system) {
    return;
  }

  m_window_system->setFocusMode(false);
  float relative_x = 0.0f;
  float relative_y = 0.0f;
  SDL_GetRelativeMouseState(&relative_x, &relative_y);
}

Vec2 EditorCamera::getCurrentCursorWindowPosition() const {
  float cursor_x = 0.0f;
  float cursor_y = 0.0f;
  SDL_GetMouseState(&cursor_x, &cursor_y);
  return Vec2(cursor_x, cursor_y);
}

bool EditorCamera::isViewportReady() const {
  return m_viewport_width > 0.0f && m_viewport_height > 0.0f;
}

bool EditorCamera::isCursorInViewport() const {
  return isWindowPositionInViewport(getCurrentCursorWindowPosition());
}

void EditorCamera::applyFreeLookRotation(const Vec2& mouse_delta) {
  if (mouse_delta.x == 0.0f && mouse_delta.y == 0.0f) {
    return;
  }

  m_yaw -= mouse_delta.x * k_free_look_rotate_speed;
  m_pitch -= mouse_delta.y * k_free_look_rotate_speed;
  m_pitch = std::clamp(m_pitch, k_min_pitch, k_max_pitch);
  updateDirectionVectors();
}

void EditorCamera::updateDirectionVectors() {
  const Vec3 world_up(0.0f, 0.0f, 1.0f);
  m_forward_direction.x = cos(m_pitch) * cos(m_yaw);
  m_forward_direction.y = cos(m_pitch) * sin(m_yaw);
  m_forward_direction.z = sin(m_pitch);
  m_forward_direction = glm::normalize(m_forward_direction);

  m_right_direction = glm::normalize(glm::cross(m_forward_direction, world_up));
  m_up_direction = glm::normalize(glm::cross(m_right_direction, m_forward_direction));
}

void EditorCamera::updateFreeLook(float delta_time, const bool* keyboard_state) {
  if (!keyboard_state) {
    return;
  }

  const Vec3 world_up(0.0f, 0.0f, 1.0f);
  float move_speed = k_free_look_move_speed;
  if (keyboard_state[SDL_SCANCODE_LSHIFT] ||
      keyboard_state[SDL_SCANCODE_RSHIFT]) {
    move_speed *= k_free_look_sprint_multiplier;
  }

  Vec3 movement(0.0f, 0.0f, 0.0f);
  if (keyboard_state[SDL_SCANCODE_W]) {
    movement += m_forward_direction;
  }
  if (keyboard_state[SDL_SCANCODE_S]) {
    movement -= m_forward_direction;
  }
  if (keyboard_state[SDL_SCANCODE_D]) {
    movement += m_right_direction;
  }
  if (keyboard_state[SDL_SCANCODE_A]) {
    movement -= m_right_direction;
  }
  if (keyboard_state[SDL_SCANCODE_E]) {
    movement += world_up;
  }
  if (keyboard_state[SDL_SCANCODE_Q]) {
    movement -= world_up;
  }

  if (movement == Vec3(0.0f, 0.0f, 0.0f)) {
    return;
  }

  m_focal_point += glm::normalize(movement) * move_speed * delta_time;
}

void EditorCamera::pan() {
  const float distance_factor = std::max(m_distance, 1.0f);
  const float pan_scale = k_pan_speed * distance_factor;
  const float pan_x = -m_mouse_delta_accumulator.x * pan_scale;
  const float pan_y = m_mouse_delta_accumulator.y * pan_scale;
  m_focal_point += m_right_direction * pan_x;
  m_focal_point += m_up_direction * pan_y;
  LOG_DEBUG("[EditorCamera] pan focal=({}, {}, {})", m_focal_point.x,
            m_focal_point.y, m_focal_point.z);
  m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
}

void EditorCamera::zoom() {
  if (m_scroll_delta_accumulator == 0.0f) {
    return;
  }

  m_focal_point += m_forward_direction * (m_scroll_delta_accumulator * k_dolly_speed);
  LOG_DEBUG("[EditorCamera] dolly focal=({}, {}, {})", m_focal_point.x,
            m_focal_point.y, m_focal_point.z);
  m_scroll_delta_accumulator = 0.0f;
}

void EditorCamera::updateViewMatrix() {
  updateDirectionVectors();
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
