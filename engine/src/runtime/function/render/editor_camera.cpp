#include "runtime/function/render/editor_camera.h"

#include <algorithm>
#include <cstdio>

#include <SDL3/SDL.h>
#include "runtime/function/slint/window_pointer_map.h"
#include <SDL3/SDL_scancode.h>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/core/event/application_event.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/event/key_event.h"
#include "runtime/core/event/mouse_event.h"
#include "runtime/platform/window/window_system.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

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

    if (m_interaction_mode == InteractionMode::pan) {
      m_is_animating_params = false; // Interrupted by pan
      pan();
    } else if (keyboard_state &&
               (m_interaction_mode == InteractionMode::free_look ||
                isCursorInViewport())) {
      bool user_active = (m_interaction_mode == InteractionMode::free_look);
      if (keyboard_state[SDL_SCANCODE_W] || keyboard_state[SDL_SCANCODE_S] ||
          keyboard_state[SDL_SCANCODE_A] || keyboard_state[SDL_SCANCODE_D] ||
          keyboard_state[SDL_SCANCODE_Q] || keyboard_state[SDL_SCANCODE_E]) {
        user_active = true;
      }
      if (user_active) {
        m_is_animating_params = false; // Interrupted by movement
      }
      applyKeyboardFlyMovement(delta_time, keyboard_state);
    }
  }

  // Update projection transition progress
  const float proj_duration = 0.4f;
  bool proj_dirty = false;
  if (m_target_projection_mode == ProjectionMode::orthographic) {
    if (m_projection_transition_t < 1.0f) {
      m_projection_transition_t += delta_time / proj_duration;
      if (m_projection_transition_t >= 1.0f) {
        m_projection_transition_t = 1.0f;
        m_projection_mode = ProjectionMode::orthographic;
      }
      proj_dirty = true;
    }
  } else {
    if (m_projection_transition_t > 0.0f) {
      m_projection_transition_t -= delta_time / proj_duration;
      if (m_projection_transition_t <= 0.0f) {
        m_projection_transition_t = 0.0f;
        m_projection_mode = ProjectionMode::perspective;
      }
      proj_dirty = true;
    }
  }

  // Update parameter transitions
  bool view_dirty = false;
  if (m_is_animating_params) {
    m_param_transition_time += delta_time;
    float t = m_param_transition_time / m_param_transition_duration;
    if (t >= 1.0f) {
      t = 1.0f;
      m_is_animating_params = false;
    }

    const float smooth_t = t * t * (3.0f - 2.0f * t); // Cubic ease-in-out

    m_focal_point = glm::mix(m_start_focal_point, m_target_focal_point, smooth_t);
    m_distance = glm::mix(m_start_distance, m_target_distance, smooth_t);

    float pitch_diff = m_target_pitch - m_start_pitch;
    m_pitch = m_start_pitch + pitch_diff * smooth_t;

    float yaw_diff = m_target_yaw - m_start_yaw;
    while (yaw_diff < -glm::pi<float>()) yaw_diff += glm::two_pi<float>();
    while (yaw_diff > glm::pi<float>()) yaw_diff -= glm::two_pi<float>();
    m_yaw = m_start_yaw + yaw_diff * smooth_t;

    if (m_projection_mode == ProjectionMode::orthographic || m_target_projection_mode == ProjectionMode::orthographic) {
      const float half_fov_tan = std::tan(m_vertical_fov * 0.5f);
      m_ortho_size = 2.0f * m_distance * std::max(half_fov_tan, 1e-4f);
      m_ortho_size = std::clamp(m_ortho_size, 0.1f, 2000.0f);
      proj_dirty = true;
    }

    view_dirty = true;
  }

  zoom();

  updateViewMatrix();

  if (proj_dirty) {
    updateProjectionMatrix();
  }
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
    if (g_runtime_global_context.m_scene_system != nullptr) {
      SceneInstance* active_scene =
          g_runtime_global_context.m_scene_system->getActiveInstance();
      if (active_scene != nullptr && active_scene->hasWorldBounds()) {
        focusOnAABB(active_scene->getWorldBounds());
        return true;
      }
    }
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

  const eastl::array<float, 2> logical =
      mapWindowPointerToLogical(m_window_system, window_position.x, window_position.y);
  const float min_x = static_cast<float>(m_viewport_origin_x);
  const float min_y = static_cast<float>(m_viewport_origin_y);
  const float max_x = min_x + m_viewport_width;
  const float max_y = min_y + m_viewport_height;
  return logical[0] >= min_x && logical[0] <= max_x && logical[1] >= min_y &&
         logical[1] <= max_y;
}

Vec2 EditorCamera::windowToViewportLocal(const Vec2& window_position) const {
  if (!isViewportReady()) {
    return window_position;
  }

  const eastl::array<float, 2> logical =
      mapWindowPointerToLogical(m_window_system, window_position.x, window_position.y);
  return Vec2(logical[0] - static_cast<float>(m_viewport_origin_x),
              logical[1] - static_cast<float>(m_viewport_origin_y));
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
  if (m_target_projection_mode == mode) {
    return;
  }

  const float half_fov_tan = std::tan(m_vertical_fov * 0.5f);
  if (mode == ProjectionMode::orthographic) {
    // Match the visible world height from the current perspective orbit.
    const float visible_height =
        2.0f * m_distance * std::max(half_fov_tan, 1e-4f);
    m_ortho_size = std::clamp(visible_height, 0.5f, 2000.0f);
  }

  m_target_projection_mode = mode;
  updateProjectionMatrix();

  LOG_INFO("[EditorCamera] projection mode transition started to {} (distance={:.2f}, ortho_size={:.2f})",
           m_target_projection_mode == ProjectionMode::perspective ? "perspective"
                                                                   : "orthographic",
           m_distance, m_ortho_size);
}

void EditorCamera::toggleProjectionMode() {
  setProjectionMode(m_target_projection_mode == ProjectionMode::perspective
                        ? ProjectionMode::orthographic
                        : ProjectionMode::perspective);
}

void EditorCamera::startParamAnimation(const Vec3& target_focal_point,
                                       float target_distance,
                                       float target_pitch,
                                       float target_yaw) {
  m_is_animating_params = true;
  m_param_transition_time = 0.0f;
  m_param_transition_duration = 0.4f;

  m_start_focal_point = m_focal_point;
  m_target_focal_point = target_focal_point;

  m_start_distance = m_distance;
  m_target_distance = target_distance;

  m_start_pitch = m_pitch;
  m_target_pitch = target_pitch;

  m_start_yaw = m_yaw;
  m_target_yaw = target_yaw;
}

void EditorCamera::alignToAxisView(uint32_t axis_index, bool positive) {
  if (axis_index > 2) {
    return;
  }

  Vec3 world_axis(0.0f);
  world_axis[static_cast<int>(axis_index)] = 1.0f;
  const Vec3 view_forward = positive ? -world_axis : world_axis;

  float target_pitch = std::asin(std::clamp(view_forward.z, -1.0f, 1.0f));
  float target_yaw = std::atan2(view_forward.y, view_forward.x);
  target_pitch = std::clamp(target_pitch, k_min_pitch, k_max_pitch);

  startParamAnimation(m_focal_point, m_distance, target_pitch, target_yaw);
  setProjectionMode(ProjectionMode::orthographic); // Switch smoothly to ortho view

  static constexpr const char* k_axis_names[3] = {"X", "Y", "Z"};
  LOG_INFO("[EditorCamera] aligned smoothly to {}{} axis view",
           positive ? "+" : "-", k_axis_names[axis_index]);
}

void EditorCamera::alignToIsometricView() {
  // Classic dimetric elevation on Z-up XY ground (Unity scene-view Iso).
  constexpr float k_iso_elevation = 0.615479709686432f;  // atan(sqrt(2)) ≈ 35.264°
  float target_yaw = glm::radians(45.0f);
  float target_pitch = -k_iso_elevation;
  target_pitch = std::clamp(target_pitch, k_min_pitch, k_max_pitch);

  startParamAnimation(m_focal_point, m_distance, target_pitch, target_yaw);
  LOG_INFO("[EditorCamera] aligned smoothly to isometric view (yaw=45°, pitch={:.1f}°)",
           glm::degrees(target_pitch));
}

void EditorCamera::alignToDefaultPerspectiveView() {
  // Keep yaw; reset elevation to editor default (+30°) so Persp ≠ Iso oblique.
  constexpr float k_default_pitch = glm::radians(30.0f);
  float target_pitch = k_default_pitch;
  target_pitch = std::clamp(target_pitch, k_min_pitch, k_max_pitch);

  startParamAnimation(m_focal_point, m_distance, target_pitch, m_yaw);
  LOG_INFO("[EditorCamera] aligned smoothly to default perspective view (pitch={:.1f}°)",
           glm::degrees(target_pitch));
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

  m_is_animating_params = false; // Interrupted by free look mouse movement
  m_yaw -= mouse_delta.x * k_free_look_rotate_speed;
  m_pitch -= mouse_delta.y * k_free_look_rotate_speed;
  m_pitch = std::clamp(m_pitch, k_min_pitch, k_max_pitch);
  updateDirectionVectors();
}

void EditorCamera::updateDirectionVectors() {
  const Vec3 world_up = kWorldUp;
  m_forward_direction.x = cos(m_pitch) * cos(m_yaw);
  m_forward_direction.y = cos(m_pitch) * sin(m_yaw);
  m_forward_direction.z = sin(m_pitch);
  m_forward_direction = glm::normalize(m_forward_direction);

  m_right_direction = glm::normalize(glm::cross(m_forward_direction, world_up));
  m_up_direction = glm::normalize(glm::cross(m_right_direction, m_forward_direction));
}

void EditorCamera::applyKeyboardFlyMovement(float delta_time,
                                          const bool* keyboard_state) {
  if (!keyboard_state) {
    return;
  }

  const Vec3 world_up = kWorldUp;
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
  m_is_animating_params = false; // Interrupted by pan
  const float distance_factor =
      m_projection_mode == ProjectionMode::orthographic
          ? std::max(m_ortho_size, 1.0f)
          : std::max(m_distance, 1.0f);
  const float pan_scale = k_pan_speed * distance_factor;
  const float pan_x = -m_mouse_delta_accumulator.x * pan_scale;
  const float pan_y = m_mouse_delta_accumulator.y * pan_scale;
  m_focal_point += m_right_direction * pan_x;
  m_focal_point += m_up_direction * pan_y;
  m_mouse_delta_accumulator = Vec2(0.0f, 0.0f);
}

void EditorCamera::zoom() {
  if (m_scroll_delta_accumulator == 0.0f) {
    return;
  }

  m_is_animating_params = false; // Interrupted by zoom
  const float zoom_factor = 1.0f - m_scroll_delta_accumulator * 0.1f;
  m_distance *= std::max(zoom_factor, 0.01f);
  m_distance = std::clamp(m_distance, 0.05f, 2000.0f);

  if (m_projection_mode == ProjectionMode::orthographic) {
    const float half_fov_tan = std::tan(m_vertical_fov * 0.5f);
    m_ortho_size = 2.0f * m_distance * std::max(half_fov_tan, 1e-4f);
    m_ortho_size = std::clamp(m_ortho_size, 0.1f, 2000.0f);
    updateProjectionMatrix();
  }
  m_scroll_delta_accumulator = 0.0f;
}

void EditorCamera::updateViewMatrix() {
  updateDirectionVectors();
  m_position = m_focal_point - m_forward_direction * m_distance;
  m_view_matrix = glm::lookAt(m_position, m_focal_point, m_up_direction);
}

void EditorCamera::updateProjectionMatrix() {
  const float aspect = m_viewport_width / m_viewport_height;

  // Calculate perspective projection matrix
  Mat4 persp = glm::perspectiveZO(m_vertical_fov, aspect, m_near_clip, m_far_clip);
  persp[1][1] *= -1.0f;

  // Calculate matching orthographic projection matrix
  const float half_fov_tan = std::tan(m_vertical_fov * 0.5f);
  const float target_ortho_size = 2.0f * m_distance * std::max(half_fov_tan, 1e-4f);
  const float ortho_half_h = target_ortho_size * 0.5f;
  const float ortho_half_w = ortho_half_h * aspect;
  Mat4 ortho = glm::orthoZO(-ortho_half_w, ortho_half_w, -ortho_half_h, ortho_half_h,
                            m_near_clip, m_far_clip);
  ortho[1][1] *= -1.0f;

  // Cubic ease-in-out easing for projection transition
  float t = m_projection_transition_t;
  float smooth_t = t * t * (3.0f - 2.0f * t);

  m_projection_matrix = (1.0f - smooth_t) * persp + smooth_t * ortho;
}

void EditorCamera::focusOnAABB(const AABB& bounds) {
  const glm::vec3 extents = bounds.extents();
  const float radius = glm::length(extents);
  float target_distance = std::max(radius * 2.5f, 1.0f);
  Vec3 target_focal_point = bounds.center();

  startParamAnimation(target_focal_point, target_distance, m_pitch, m_yaw);
  LOG_INFO("[EditorCamera] focusing smoothly on AABB center=({}, {}, {}) distance={}",
           target_focal_point.x, target_focal_point.y, target_focal_point.z, target_distance);
}

void EditorCamera::setLookAt(const Vec3& position, const Vec3& target) {
  Vec3 forward = target - position;
  const float distance = glm::distance(position, target);
  if (distance < 1e-4f) {
    return;
  }

  forward /= distance;
  float target_pitch = std::asin(std::clamp(forward.z, -1.0f, 1.0f));
  float target_yaw = std::atan2(forward.y, forward.x);

  startParamAnimation(target, distance, target_pitch, target_yaw);
  LOG_INFO("[EditorCamera] look-at position smoothly transition to Target=({}, {}, {}) Position=({}, {}, {})",
           target.x, target.y, target.z, position.x, position.y, position.z);
}

void EditorCamera::placeInsideAABB(const AABB& bounds) {
  constexpr float k_eye_height = 1.7f;
  constexpr float k_look_ahead = 6.0f;

  const Vec3 center = bounds.center();
  const float eye_z = bounds.min.z + k_eye_height;
  const Vec3 position(center.x, center.y, eye_z);

  Vec3 look_target = center;
  const Vec3 size = bounds.size();
  if (size.x >= size.y) {
    look_target = Vec3(center.x + k_look_ahead, center.y, eye_z);
  } else {
    look_target = Vec3(center.x, center.y + k_look_ahead, eye_z);
  }

  setLookAt(position, look_target);
}

}  // namespace Blunder
