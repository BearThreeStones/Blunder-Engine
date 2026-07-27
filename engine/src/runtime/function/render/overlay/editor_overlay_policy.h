#pragma once

#include "runtime/function/global/engine_host_mode.h"

namespace Blunder {

/// Authorship viewport chrome (grid, Transform/Navigate gizmos, outline, …).
/// Disabled for the Player host — including while Play Pause is active.
inline bool editorOverlaysEnabled(EngineHostMode host_mode) {
  return host_mode != EngineHostMode::Player;
}

}  // namespace Blunder
