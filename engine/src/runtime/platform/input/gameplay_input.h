#pragma once

namespace Blunder {

struct GameplayInputKeys {
  bool w{false};
  bool a{false};
  bool s{false};
  bool d{false};
  bool space{false};
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
