#include "runtime/core/reflection/lifecycle.h"

#include "EASTL/string.h"
#include "EASTL/unordered_map.h"

namespace Blunder {
namespace {

struct Hooks {
  LifecycleTickFn tick{nullptr};
  LifecycleReadyFn ready{nullptr};
};

eastl::unordered_map<eastl::string, Hooks>& hooks() {
  static eastl::unordered_map<eastl::string, Hooks> s_hooks;
  return s_hooks;
}

}  // namespace

void LifecycleDispatch::clear() { hooks().clear(); }

void LifecycleDispatch::setTickHook(const char* class_name,
                                    LifecycleTickFn fn) {
  if (class_name == nullptr) {
    return;
  }
  hooks()[class_name].tick = fn;
}

void LifecycleDispatch::setReadyHook(const char* class_name,
                                     LifecycleReadyFn fn) {
  if (class_name == nullptr) {
    return;
  }
  hooks()[class_name].ready = fn;
}

void LifecycleDispatch::invokeTick(Object* object, float delta_time) {
  if (object == nullptr || object->getScriptPeer() == nullptr) {
    return;
  }
  // Pilot: all Objects use the "Object" type hook table.
  const auto it = hooks().find("Object");
  if (it == hooks().end() || it->second.tick == nullptr) {
    return;
  }
  it->second.tick(object->getScriptPeer(), delta_time);
}

void LifecycleDispatch::invokeReady(Object* object) {
  if (object == nullptr || object->getScriptPeer() == nullptr) {
    return;
  }
  const auto it = hooks().find("Object");
  if (it == hooks().end() || it->second.ready == nullptr) {
    return;
  }
  it->second.ready(object->getScriptPeer());
}

}  // namespace Blunder
