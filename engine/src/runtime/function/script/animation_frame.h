#pragma once

#include "runtime/core/object/object.h"
#include "runtime/function/script/play_tick_gate.h"

namespace Blunder {

/// Play Mode frame: Behaviour Ready/Tick → AnimationPlayer advance/sample → PoseApplied.
inline void tickObjectAnimationPlayFrame(Object* object, float delta_time,
                                          bool play_paused) {
  if (object == nullptr) {
    return;
  }
  dispatchObjectLifecycle(object, delta_time, play_paused);
  if (play_paused) {
    return;
  }
  AnimationPlayer* player = object->getAnimationPlayer();
  if (player == nullptr) {
    return;
  }
  player->advance(delta_time);
}

/// Edit preview: advance/sample only (no Behaviour Tick).
inline void tickObjectAnimationPreviewFrame(Object* object, float delta_time) {
  if (object == nullptr) {
    return;
  }
  AnimationPlayer* player = object->getAnimationPlayer();
  if (player == nullptr) {
    return;
  }
  player->advance(delta_time);
}

}  // namespace Blunder
