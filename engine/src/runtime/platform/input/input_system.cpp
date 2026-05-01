#include "runtime/platform/input/input_system.h"

#include "runtime/core/base/macro.h"
#include "runtime/engine.h"
#include "runtime/function/global/global_context.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

unsigned int k_complement_control_command = 0xFFFFFFFF;

void InputSystem::initialize() {
  // Cache keyboard state pointer (valid for application lifetime)
  m_keyboard_state = SDL_GetKeyboardState(nullptr);
  ASSERT(m_keyboard_state);

  LOG_INFO("[InputSystem::initialize] SDL3 input system initialized");
}

void InputSystem::shutdown() {
  m_keyboard_state = nullptr;
  LOG_INFO("[InputSystem::shutdown] SDL3 input system shut down");
}

void InputSystem::pollKeyboardState() {
  // Clear jump command each frame (one-shot behavior)
  m_game_command &=
      (k_complement_control_command ^ (unsigned int)GameCommand::jump);

  // Skip input processing in editor mode
  if (g_is_editor_mode) {
    return;
  }

  // Poll keyboard state using SDL scancodes
  // Movement keys (continuous)
  if (m_keyboard_state[SDL_SCANCODE_W]) {
    m_game_command |= (unsigned int)GameCommand::forward;
  } else {
    m_game_command &=
        (k_complement_control_command ^ (unsigned int)GameCommand::forward);
  }

  if (m_keyboard_state[SDL_SCANCODE_S]) {
    m_game_command |= (unsigned int)GameCommand::backward;
  } else {
    m_game_command &=
        (k_complement_control_command ^ (unsigned int)GameCommand::backward);
  }

  if (m_keyboard_state[SDL_SCANCODE_A]) {
    m_game_command |= (unsigned int)GameCommand::left;
  } else {
    m_game_command &=
        (k_complement_control_command ^ (unsigned int)GameCommand::left);
  }

  if (m_keyboard_state[SDL_SCANCODE_D]) {
    m_game_command |= (unsigned int)GameCommand::right;
  } else {
    m_game_command &=
        (k_complement_control_command ^ (unsigned int)GameCommand::right);
  }

  // Jump (one-shot, set on press)
  if (m_keyboard_state[SDL_SCANCODE_SPACE]) {
    m_game_command |= (unsigned int)GameCommand::jump;
  }

  // Squat (continuous)
  if (m_keyboard_state[SDL_SCANCODE_LCTRL]) {
    m_game_command |= (unsigned int)GameCommand::squat;
  } else {
    m_game_command &=
        (k_complement_control_command ^ (unsigned int)GameCommand::squat);
  }

  // Sprint (continuous)
  if (m_keyboard_state[SDL_SCANCODE_LSHIFT]) {
    m_game_command |= (unsigned int)GameCommand::sprint;
  } else {
    m_game_command &=
        (k_complement_control_command ^ (unsigned int)GameCommand::sprint);
  }

  // Free camera toggle (F key) - edge detection for toggle behavior
  bool f_key_pressed = m_keyboard_state[SDL_SCANCODE_F];
  if (f_key_pressed && !m_f_key_was_pressed) {
    m_game_command ^= (unsigned int)GameCommand::free_carema;
  }
  m_f_key_was_pressed = f_key_pressed;

  // Focus mode toggle (LEFT ALT) - edge detection for toggle behavior
  bool alt_key_pressed = m_keyboard_state[SDL_SCANCODE_LALT];
  if (alt_key_pressed && !m_alt_key_was_pressed) {
    eastl::shared_ptr<WindowSystem> window_system =
        g_runtime_global_context.m_window_system;
    window_system->setFocusMode(!window_system->getFocusMode());
  }
  m_alt_key_was_pressed = alt_key_pressed;
}

void InputSystem::pollMouseState() {
  float current_x, current_y;
  SDL_GetMouseState(&current_x, &current_y);

  if (m_first_mouse_poll) {
    m_last_cursor_x = current_x;
    m_last_cursor_y = current_y;
    m_first_mouse_poll = false;
    return;
  }

  // Only track delta when in focus mode (cursor captured)
  if (g_runtime_global_context.m_window_system->getFocusMode()) {
    m_cursor_delta_x = static_cast<int>(m_last_cursor_x - current_x);
    m_cursor_delta_y = static_cast<int>(m_last_cursor_y - current_y);
  }

  m_last_cursor_x = current_x;
  m_last_cursor_y = current_y;
}

void InputSystem::clear() {
  m_cursor_delta_x = 0;
  m_cursor_delta_y = 0;
}

void InputSystem::calculateCursorDeltaAngles() {
  eastl::array<int, 2> window_size =
      g_runtime_global_context.m_window_system->getWindowSize();

  if (window_size[0] < 1 || window_size[1] < 1) {
    return;
  }

  Radian cursor_delta_x(Math::degreesToRadians(m_cursor_delta_x));
  Radian cursor_delta_y(Math::degreesToRadians(m_cursor_delta_y));

  // FOV-based angle calculation (commented out until RenderCamera is available)
  // std::shared_ptr<RenderCamera> render_camera =
  //     g_runtime_global_context.m_render_system->getRenderCamera();
  // const Vector2& fov = render_camera->getFOV();
  // m_cursor_delta_yaw = (cursor_delta_x / (float)window_size[0]) * fov.x;
  // m_cursor_delta_pitch = -(cursor_delta_y / (float)window_size[1]) * fov.y;
}

void InputSystem::tick() {
  pollKeyboardState();
  pollMouseState();
  calculateCursorDeltaAngles();
  clear();

  // Update invalid flag based on window focus
  eastl::shared_ptr<WindowSystem> window_system =
      g_runtime_global_context.m_window_system;
  if (window_system->getFocusMode()) {
    m_game_command &=
        (k_complement_control_command ^ (unsigned int)GameCommand::invalid);
  } else {
    m_game_command |= (unsigned int)GameCommand::invalid;
  }
}

}  // namespace Blunder
