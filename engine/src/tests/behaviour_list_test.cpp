#include "runtime/core/object/object_db.h"

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

  ObjectDB::destroy(oid);
  ObjectDB::clear();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
