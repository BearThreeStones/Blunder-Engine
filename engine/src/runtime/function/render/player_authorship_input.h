#pragma once

#include "runtime/function/global/engine_host_mode.h"

namespace Blunder {

/// Editor Camera, pick, gizmos, and other authorship input in the Player.
inline bool playerAuthorshipInputEnabled(EngineHostMode host_mode) {
  return host_mode != EngineHostMode::Player;
}

}  // namespace Blunder
