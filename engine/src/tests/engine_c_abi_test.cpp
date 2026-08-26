#include "runtime/core/log/console_ring.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/core/reflection/lifecycle.h"

#include <cmath>
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

  expect_true("abi version >= 12", blunder_engine_abi_version() >= 12);
  expect_true("abi version matches header",
              blunder_engine_abi_version() == BLUNDER_ENGINE_C_ABI_VERSION);

  ConsoleRing::instance().clear();
  expect_true(
      "blunder_log warning",
      blunder_log(BLUNDER_LOG_SEVERITY_WARNING, "clip missing", nullptr) ==
          BLUNDER_ENGINE_OK);
  const auto logged = ConsoleRing::instance().snapshot();
  expect_true("log row present", logged.size() == 1);
  if (!logged.empty()) {
    expect_true("log severity warning",
                logged[0].severity == ConsoleSeverity::Warning);
    expect_true("log text", logged[0].text == "clip missing");
    expect_true("log stack empty", logged[0].stack.empty());
  }

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

  const BlunderBehaviourId bid = blunder_object_add_behaviour(id, "Motor");
  expect_true("add behaviour", bid != 0);
  expect_true("count", blunder_object_behaviour_count(id) == 1);
  expect_true("id at 0", blunder_object_behaviour_id_at(id, 0) == bid);
  int peer = 0;
  expect_true("set peer",
              blunder_object_set_behaviour_peer(id, bid, &peer) ==
                  BLUNDER_ENGINE_OK);
  expect_true("get peer",
              blunder_object_get_behaviour_peer(id, bid) == &peer);
  expect_true("set pos",
              blunder_object_set_vec3_property(id, "Object", "position", 1, 2,
                                              3) == BLUNDER_ENGINE_OK);
  float x = 0, y = 0, z = 0;
  expect_true("get pos",
              blunder_object_get_vec3_property(id, "Object", "position", &x, &y,
                                              &z) == BLUNDER_ENGINE_OK);
  expect_true("pos values", x == 1.f && y == 2.f && z == 3.f);

  expect_true("set rot",
              blunder_object_set_quat_property(id, "Object", "rotation", 0.f, 0.f,
                                              0.70710677f, 0.70710677f) ==
                  BLUNDER_ENGINE_OK);
  float qx = 0, qy = 0, qz = 0, qw = 0;
  expect_true("get rot",
              blunder_object_get_quat_property(id, "Object", "rotation", &qx,
                                              &qy, &qz, &qw) ==
                  BLUNDER_ENGINE_OK);
  expect_true("rot values", std::fabs(qx) < 1e-5f && std::fabs(qy) < 1e-5f &&
                                std::fabs(qz - 0.70710677f) < 1e-5f &&
                                std::fabs(qw - 0.70710677f) < 1e-5f);

  expect_true("vec3 cannot set rotation",
              blunder_object_set_vec3_property(id, "Object", "rotation", 1, 2,
                                              3) == BLUNDER_ENGINE_ERROR);
  qx = qy = qz = qw = 0;
  expect_true("rotation unchanged after vec3 reject",
              blunder_object_get_quat_property(id, "Object", "rotation", &qx,
                                              &qy, &qz, &qw) ==
                      BLUNDER_ENGINE_OK &&
                  std::fabs(qx) < 1e-5f && std::fabs(qy) < 1e-5f &&
                  std::fabs(qz - 0.70710677f) < 1e-5f &&
                  std::fabs(qw - 0.70710677f) < 1e-5f);

  expect_true("zero quat",
              blunder_object_set_quat_property(id, "Object", "rotation", 0.f, 0.f,
                                              0.f, 0.f) == BLUNDER_ENGINE_OK);
  qx = qy = qz = qw = 0;
  expect_true("zero quat becomes identity",
              blunder_object_get_quat_property(id, "Object", "rotation", &qx,
                                              &qy, &qz, &qw) ==
                      BLUNDER_ENGINE_OK &&
                  std::fabs(qx) < 1e-5f && std::fabs(qy) < 1e-5f &&
                  std::fabs(qz) < 1e-5f && std::fabs(qw - 1.f) < 1e-5f);

  eastl::string name = "ViaCAbi";
  const void* args[1] = {&name};
  expect_true("ptrcall",
              blunder_ptrcall("Object", "set_name", id, args, nullptr) ==
                  BLUNDER_ENGINE_OK);
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  expect_true("name set", object != nullptr && object->getName() == "ViaCAbi");

  int ready_peer_hits = 0;
  auto ready_hook = [](void* peer_ptr) {
    *static_cast<int*>(peer_ptr) += 1;
  };
  expect_true("set ready hook",
              blunder_lifecycle_set_ready_hook("Object", ready_hook) ==
                  BLUNDER_ENGINE_OK);
  if (object != nullptr) {
    const BlunderBehaviourId ready_bid =
        blunder_object_add_behaviour(id, "Object");
    expect_true("ready behaviour", ready_bid != 0);
    expect_true("set ready peer",
                blunder_object_set_behaviour_peer(id, ready_bid,
                                                  &ready_peer_hits) ==
                    BLUNDER_ENGINE_OK);
    LifecycleDispatch::invokeReady(object);
    expect_true("ready invoked", ready_peer_hits == 1);
  }

  expect_true("remove behaviour",
              blunder_object_remove_behaviour(id, bid) == BLUNDER_ENGINE_OK);
  expect_true("count after remove",
              blunder_object_behaviour_count(id) == 1);

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
