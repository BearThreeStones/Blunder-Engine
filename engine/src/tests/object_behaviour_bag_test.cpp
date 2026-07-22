// engine/src/tests/object_behaviour_bag_test.cpp
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/variant.h"
#include "runtime/function/scene/scene.h"

#include <cstdio>

namespace {
int g_failures = 0;
void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}
}  // namespace

int main() {
  using namespace Blunder;
  ObjectDB::clear();
  ObjectId oid = ObjectDB::create();
  Object* object = ObjectDB::get(oid);
  expect_true("object", object != nullptr);
  BehaviourId id = object->addBehaviour("Probe.Motor");
  expect_true("id", isValid(id));

  eastl::vector<SceneBehaviourProperty> bag;
  SceneBehaviourProperty p;
  p.key = "Speed";
  p.value = Variant(1.5f);
  bag.push_back(p);
  expect_true("set bag", object->setBehaviourProperties(id, bag));
  const auto* got = object->getBehaviourProperties(id);
  expect_true("get bag", got != nullptr && got->size() == 1 &&
                             (*got)[0].key == "Speed");

  BehaviourId id2 = object->addBehaviour("Probe.Bark");
  expect_true("two", object->getBehaviourCount() == 2);
  expect_true("order0", object->getBehaviourIdAt(0) == id);
  expect_true("move", object->moveBehaviour(0, 2));  // move first to end
  expect_true("order after", object->getBehaviourIdAt(0) == id2 &&
                                 object->getBehaviourIdAt(1) == id);

  if (g_failures) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("object_behaviour_bag_test: OK\n");
  return 0;
}
