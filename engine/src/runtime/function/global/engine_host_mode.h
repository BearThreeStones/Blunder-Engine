#pragma once

#include <cstdint>

namespace Blunder {

enum class EngineHostMode : uint8_t {
  Editor = 0,
  Player = 1,
};

/// Headless is a bool beside host mode — not `EngineHostMode::Headless`.
inline bool hostCreatesOsWindow(bool headless) { return !headless; }

inline bool hostMountsEditorShell(EngineHostMode mode, bool headless) {
  return mode == EngineHostMode::Editor && !headless;
}

inline bool hostMountsPlaySession(EngineHostMode mode) {
  return mode == EngineHostMode::Editor;
}

}  // namespace Blunder
