#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/lifecycle.h"

#include <cstdio>

namespace {
int g_failures = 0;
void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void on_tick(void* peer, float dt) {
  *static_cast<int*>(peer) += 1;
  (void)dt;
}

void on_ready(void* peer) { *static_cast<int*>(peer) += 1; }

int g_foreach_count = 0;
void on_foreach(Blunder::Object* object) {
  (void)object;
  ++g_foreach_count;
}
}  // namespace

int main() {
  using namespace Blunder;
  ObjectDB::clear();
  ObjectId oid = ObjectDB::create();
  Object* object = ObjectDB::get(oid);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return 1;
  }

  expect_true("empty", object->getBehaviourCount() == 0);
  BehaviourId a = object->addBehaviour("Motor");
  BehaviourId b = object->addBehaviour("Bark");
  BehaviourId c = object->addBehaviour("Motor");  // duplicate type allowed
  expect_true("count 3", object->getBehaviourCount() == 3);
  expect_true("ids distinct", a != b && b != c && a != c);
  expect_true("order 0 Motor",
              eastl::string(object->getBehaviourTypeName(a)) == "Motor");
  expect_true("order 1 Bark",
              eastl::string(object->getBehaviourTypeName(
                  object->getBehaviourIdAt(1))) == "Bark");

  int peer_a = 1;
  object->setBehaviourScriptPeer(a, &peer_a);
  expect_true("peer a", object->getBehaviourScriptPeer(a) == &peer_a);

  expect_true("remove b", object->removeBehaviour(b));
  expect_true("count 2", object->getBehaviourCount() == 2);
  expect_true("b gone", object->getBehaviourScriptPeer(b) == nullptr);

  // Multi-peer Tick: both non-null peers receive invokeTick in list order.
  int peer0 = 0;
  int peer1 = 0;
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer0);
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(1), &peer1);
  LifecycleDispatch::clear();
  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::invokeTick(object, 0.016f);
  expect_true("both peers ticked", peer0 == 1 && peer1 == 1);

  // Null peers are skipped; remaining peer still ticks.
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), nullptr);
  peer1 = 0;
  LifecycleDispatch::invokeTick(object, 0.016f);
  expect_true("null peer skipped", peer0 == 1 && peer1 == 1);

  // Ready-once per slot: second invokeReady must not re-fire.
  int ready0 = 0;
  int ready1 = 0;
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &ready0);
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(1), &ready1);
  LifecycleDispatch::clear();
  LifecycleDispatch::setReadyHook("Object", on_ready);
  LifecycleDispatch::invokeReady(object);
  expect_true("both peers ready", ready0 == 1 && ready1 == 1);
  LifecycleDispatch::invokeReady(object);
  expect_true("ready not re-fired", ready0 == 1 && ready1 == 1);

  // Peer reset clears ready_invoked so Ready can fire again.
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &ready0);
  LifecycleDispatch::invokeReady(object);
  expect_true("ready after peer reset", ready0 == 2 && ready1 == 1);

  ObjectDB::destroy(oid);

  // ObjectDB::forEach visits occupied Objects only.
  ObjectId o1 = ObjectDB::create();
  ObjectId o2 = ObjectDB::create();
  g_foreach_count = 0;
  ObjectDB::forEach(on_foreach);
  expect_true("forEach two objects", g_foreach_count == 2);
  ObjectDB::destroy(o1);
  g_foreach_count = 0;
  ObjectDB::forEach(on_foreach);
  expect_true("forEach after destroy", g_foreach_count == 1);
  ObjectDB::destroy(o2);

  ObjectDB::clear();
  LifecycleDispatch::clear();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
