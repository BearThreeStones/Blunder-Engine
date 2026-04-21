#pragma once

#include <SDL3/SDL.h>

#include "runtime/core/math/math.h"

namespace Blunder {

enum class GameCommand : unsigned int {
  forward = 1 << 0,                  // W
  backward = 1 << 1,                 // S
  left = 1 << 2,                     // A
  right = 1 << 3,                    // D
  jump = 1 << 4,                     // SPACE
  squat = 1 << 5,                    // LEFT CTRL
  sprint = 1 << 6,                   // LEFT SHIFT
  fire = 1 << 7,                     // not implemented yet
  free_carema = 1 << 8,              // F
  invalid = (unsigned int)(1 << 31)  // lost focus
};

extern unsigned int k_complement_control_command;

class InputSystem {
 public:
  void initialize();
  void shutdown();
  void tick();
  void clear();

  int m_cursor_delta_x{0};
  int m_cursor_delta_y{0};

  Radian m_cursor_delta_yaw{0};
  Radian m_cursor_delta_pitch{0};

  void resetGameCommand() { m_game_command = 0; }
  unsigned int getGameCommand() const { return m_game_command; }

 private:
  void pollKeyboardState();
  void pollMouseState();
  void calculateCursorDeltaAngles();

  unsigned int m_game_command{0};

  // Mouse state
  float m_last_cursor_x{0.0f};
  float m_last_cursor_y{0.0f};
  bool m_first_mouse_poll{true};

  // Key edge detection for toggles
  bool m_f_key_was_pressed{false};
  bool m_alt_key_was_pressed{false};

  // SDL keyboard state pointer (valid for app lifetime)
  const bool* m_keyboard_state{nullptr};
};

}  // namespace Blunder
