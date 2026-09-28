#pragma once

namespace Blunder {

struct GameplayInputKeys {
  bool w{false};
  bool a{false};
  bool s{false};
  bool d{false};
  /// Arrow aliases OR'd with WASD by the host before sample (same Move axis).
  bool up{false};
  bool left{false};
  bool down{false};
  bool right{false};
  bool space{false};
  /// Window keyboard focus (Player: SDL input focus, not Focus Mode / Left Alt).
  /// Informational / host diagnostics only — sample() no longer gates on it for
  /// engine_player (Editor-spawned Player often lacks foreground).
  bool focused{true};
  bool paused{false};
  bool player_host{false};
};

struct GameplayInputSnapshot {
  float move_x{0.f};
  float move_y{0.f};
  bool jump_pressed{false};
};

class GameplayInputState {
 public:
  GameplayInputSnapshot sample(const GameplayInputKeys& keys);
  GameplayInputSnapshot current() const { return m_current; }
  void reset();

 private:
  GameplayInputSnapshot m_current{};
  bool m_space_was_down{false};
};

GameplayInputState& gameplayInputState();

}  // namespace Blunder
