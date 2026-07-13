#pragma once

#include "runtime/core/object/object.h"

namespace Blunder {

using LifecycleTickFn = void (*)(void* script_peer, float delta_time);
using LifecycleReadyFn = void (*)(void* script_peer);

class LifecycleDispatch {
 public:
  static void clear();
  static void setTickHook(const char* class_name, LifecycleTickFn fn);
  static void setReadyHook(const char* class_name, LifecycleReadyFn fn);
  static void invokeTick(Object* object, float delta_time);
  static void invokeReady(Object* object);
};

}  // namespace Blunder
