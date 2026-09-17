#pragma once

#include "runtime/function/global/engine_host_mode.h"

namespace Blunder {

/// Authorship viewport chrome (grid, Transform/Navigate gizmos, outline, …).
/// Disabled for the Player host — including while Play Pause is active.
inline bool editorOverlaysEnabled(EngineHostMode host_mode) {
  return host_mode != EngineHostMode::Player;
}

/// Scene gizmos in the 3D view (transform handles, light/camera/volume wires).
/// The navigate cube / Persp-Iso HUD is not a scene gizmo.
inline bool sceneAuthorshipGizmosEnabled(EngineHostMode host_mode,
                                         bool gizmos_visible) {
  return editorOverlaysEnabled(host_mode) && gizmos_visible;
}

}  // namespace Blunder
