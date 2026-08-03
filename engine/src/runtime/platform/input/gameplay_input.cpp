#include "runtime/platform/input/gameplay_input.h"

#include "runtime/core/object/cine_segment_service.h"

#include <cmath>

namespace Blunder {

void GameplayInputState::reset() {
  m_current = {};
  m_space_was_down = false;
}

GameplayInputSnapshot GameplayInputState::sample(const GameplayInputKeys& keys) {
  const bool authoritative = keys.player_host && keys.focused && !keys.paused &&
                             !cineSegmentService().isGameplayInputSuppressed();

  if (!authoritative) {
    m_space_was_down = keys.space;
    m_current = {};
    return m_current;
  }

  float x = 0.f;
  float y = 0.f;
  if (keys.d) {
    x += 1.f;
  }
  if (keys.a) {
    x -= 1.f;
  }
  if (keys.w) {
    y += 1.f;
  }
  if (keys.s) {
    y -= 1.f;
  }
  const float len = std::sqrt(x * x + y * y);
  if (len > 1.e-6f) {
    x /= len;
    y /= len;
  }

  const bool jump = keys.space && !m_space_was_down;
  m_space_was_down = keys.space;

  m_current.move_x = x;
  m_current.move_y = y;
  m_current.jump_pressed = jump;
  return m_current;
}

GameplayInputState& gameplayInputState() {
  static GameplayInputState s;
  return s;
}

}  // namespace Blunder
