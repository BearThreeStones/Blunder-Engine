#include "runtime/core/log/log_system.h"
#include "runtime/core/object/behaviour_id.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

struct FindByEntityCtx {
  Blunder::EntityId entity_id{Blunder::k_invalid_entity_id};
  Blunder::Object* found{nullptr};
};

void onFindByEntity(Blunder::Object* object, void* user) {
  auto* ctx = static_cast<FindByEntityCtx*>(user);
  if (object != nullptr && object->getEntityId() == ctx->entity_id) {
    ctx->found = object;
  }
}

Blunder::Object* findObjectByEntityId(Blunder::EntityId entity_id) {
  FindByEntityCtx ctx;
  ctx.entity_id = entity_id;
  Blunder::ObjectDB::forEach(onFindByEntity, &ctx);
  return ctx.found;
}

/// Load scene JSON with behaviours, instantiate without DotNetHost: Object
/// slots must exist with stable ids and null peers; empty-behaviour entities
/// must not get an Object.
void instantiateRestoresBehaviourSlotsWithoutHost() {
  using namespace Blunder;
  ensureLogger();
  ObjectDB::clear();

  const char* kJson = R"({
  "type": "Scene",
  "guid": "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee",
  "entities": [
    {
      "name": "Actor",
      "position": [0, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees",
      "behaviours": [
        { "type": "Game.Motor", "id": 1 },
        { "type": "Game.Bark", "id": 3 }
      ]
    },
    {
      "name": "Prop",
      "position": [1, 0, 0],
      "rotation": [0, 0, 0],
      "rotationMode": "euler_degrees"
    }
  ]
}
)";

  Scene scene;
  expect_true("deserialize scene with behaviours",
              SceneSerializer::deserialize(eastl::string(kJson), scene));
  expect_true("two entities deserialized", scene.getEntities().size() == 2);

  SceneInstance instance;
  instance.instantiate(scene);

  const EntityId actor_id = instance.findEntityByName("Actor");
  const EntityId prop_id = instance.findEntityByName("Prop");
  expect_true("actor entity exists", isValid(actor_id));
  expect_true("prop entity exists", isValid(prop_id));

  Object* actor_object = findObjectByEntityId(actor_id);
  expect_true("actor has bound Object", actor_object != nullptr);
  if (actor_object != nullptr) {
    expect_true("actor Object entity id bound",
                actor_object->getEntityId() == actor_id);
    expect_true("two behaviour slots",
                actor_object->getBehaviourCount() == 2);
    expect_true("slot 0 id is 1",
                actor_object->getBehaviourIdAt(0) ==
                    static_cast<BehaviourId>(1));
    expect_true("slot 1 id is 3",
                actor_object->getBehaviourIdAt(1) ==
                    static_cast<BehaviourId>(3));
    expect_true(
        "slot 0 type Motor",
        eastl::string(actor_object->getBehaviourTypeName(
            static_cast<BehaviourId>(1))) == "Game.Motor");
    expect_true(
        "slot 1 type Bark",
        eastl::string(actor_object->getBehaviourTypeName(
            static_cast<BehaviourId>(3))) == "Game.Bark");
    expect_true("slot 0 peer null",
                actor_object->getBehaviourScriptPeer(
                    static_cast<BehaviourId>(1)) == nullptr);
    expect_true("slot 1 peer null",
                actor_object->getBehaviourScriptPeer(
                    static_cast<BehaviourId>(3)) == nullptr);

    const BehaviourId next =
        actor_object->addBehaviour("Game.AfterRestore");
    expect_true("next BehaviourId advanced past max restored",
                next == static_cast<BehaviourId>(4));
  }

  Object* prop_object = findObjectByEntityId(prop_id);
  expect_true("prop without behaviours has no Object",
              prop_object == nullptr);

  ObjectDB::clear();
}

}  // namespace

int main() {
  instantiateRestoresBehaviourSlotsWithoutHost();

  using namespace Blunder;
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stderr, "scene_behaviour_instantiate_test: all passed\n");
  }
  std::fflush(stderr);
  return exit_code;
}
