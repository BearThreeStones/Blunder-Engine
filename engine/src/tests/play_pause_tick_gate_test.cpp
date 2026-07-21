#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/script/play_tick_gate.h"

#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;
int g_ready_calls = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void on_tick(void* /*peer*/, float /*dt*/) { ++g_tick_calls; }

void on_ready(void* /*peer*/) { ++g_ready_calls; }

}  // namespace

int main() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  g_ready_calls = 0;

  expect_true("gate allows when not paused",
              shouldAdvanceBehaviourTick(false));
  expect_true("gate blocks when paused", !shouldAdvanceBehaviourTick(true));

  ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return 1;
  }

  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::setReadyHook("Object", on_ready);

  dispatchObjectLifecycle(object, 0.016f, /*play_paused=*/false);
  expect_true("ready while playing", g_ready_calls == 1);
  expect_true("tick while playing", g_tick_calls == 1);

  dispatchObjectLifecycle(object, 0.016f, /*play_paused=*/true);
  expect_true("ready already invoked while paused", g_ready_calls == 1);
  expect_true("tick skipped while paused", g_tick_calls == 1);

  dispatchObjectLifecycle(object, 0.016f, /*play_paused=*/false);
  expect_true("tick resumes after pause", g_tick_calls == 2);

  ObjectDB::clear();
  LifecycleDispatch::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("play_pause_tick_gate_test: all passed\n");
  return 0;
}
