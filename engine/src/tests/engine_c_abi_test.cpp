#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/core/object/object_db.h"

#include <cstdio>

#include "EASTL/string.h"

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
  ClassDB::initialize();

  expect_true("abi version", blunder_engine_abi_version() == 1);

  const BlunderObjectId id = blunder_object_create();
  expect_true("create", blunder_object_is_valid(id) == 1);

  expect_true("set enabled",
              blunder_object_set_bool_property(id, "Object", "enabled", 0) ==
                  BLUNDER_ENGINE_OK);
  int enabled = 1;
  expect_true("get enabled",
              blunder_object_get_bool_property(id, "Object", "enabled",
                                               &enabled) == BLUNDER_ENGINE_OK);
  expect_true("enabled is 0", enabled == 0);

  eastl::string name = "ViaCAbi";
  const void* args[1] = {&name};
  expect_true("ptrcall",
              blunder_ptrcall("Object", "set_name", id, args, nullptr) ==
                  BLUNDER_ENGINE_OK);
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  expect_true("name set", object != nullptr && object->getName() == "ViaCAbi");

  int ready_peer_hits = 0;
  auto ready_hook = [](void* peer) {
    *static_cast<int*>(peer) += 1;
  };
  expect_true("set ready hook",
              blunder_lifecycle_set_ready_hook("Object", ready_hook) ==
                  BLUNDER_ENGINE_OK);
  if (object != nullptr) {
    object->addBehaviour("Object");
    object->setBehaviourScriptPeer(object->getBehaviourIdAt(0),
                                   &ready_peer_hits);
    LifecycleDispatch::invokeReady(object);
    expect_true("ready invoked", ready_peer_hits == 1);
  }

  expect_true("destroy", blunder_object_destroy(id) == BLUNDER_ENGINE_OK);
  expect_true("invalid after destroy", blunder_object_is_valid(id) == 0);
  expect_true("destroy invalid",
              blunder_object_destroy(id) == BLUNDER_ENGINE_ERROR);

  expect_true("clear hooks", blunder_lifecycle_clear_hooks() == BLUNDER_ENGINE_OK);

  ClassDB::shutdown();
  ObjectDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
