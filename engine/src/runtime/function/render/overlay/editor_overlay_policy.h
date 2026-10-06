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

/// Debug draw is not authorship chrome. Editor viewports always draw
/// (including play-in-editor). Player is off unless the InGame switch is on.
/// Scene Gizmos does not gate this.
inline bool debugDrawVisible(EngineHostMode host_mode, bool ingame_enabled) {
  if (host_mode == EngineHostMode::Editor) {
    return true;
  }
  return ingame_enabled;
}

}  // namespace Blunder
