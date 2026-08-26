#pragma once

#include <cstdint>

namespace Blunder {

inline constexpr const char* k_request_play_step_requires_pause =
    "play.step_requires_pause";
inline constexpr float k_play_step_dt = 1.0f / 60.0f;
inline constexpr uint32_t k_play_step_max_ticks = 3600;

inline bool playStepAllowed(bool play_paused) { return play_paused; }

/// Advance N gameplay ticks at dt 1/60. Legal only while paused. Stays paused
/// (caller keeps the pause flag). Returns false without ticking when unpaused.
template <typename TickFn>
bool applyPlayStep(bool play_paused, uint32_t tick_count, TickFn&& tick) {
  if (!playStepAllowed(play_paused)) {
    return false;
  }
  uint32_t n = tick_count;
  if (n > k_play_step_max_ticks) {
    n = k_play_step_max_ticks;
  }
  for (uint32_t i = 0; i < n; ++i) {
    tick(k_play_step_dt);
  }
  return true;
}

}  // namespace Blunder
