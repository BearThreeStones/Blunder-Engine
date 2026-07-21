#pragma once

#include "runtime/core/object/object.h"
#include "runtime/core/reflection/lifecycle.h"

namespace Blunder {

/// Pause freezes Behaviour Tick only; Ready still runs so peers can initialize.
inline bool shouldAdvanceBehaviourTick(bool play_paused) {
  return !play_paused;
}

inline void dispatchObjectLifecycle(Object* object, float delta_time,
                                    bool play_paused) {
  LifecycleDispatch::invokeReady(object);
  if (shouldAdvanceBehaviourTick(play_paused)) {
    LifecycleDispatch::invokeTick(object, delta_time);
  }
}

}  // namespace Blunder
