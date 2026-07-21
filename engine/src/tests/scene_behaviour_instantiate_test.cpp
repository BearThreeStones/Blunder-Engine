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

Blunder::Object* findObjectByEntityId(Blunder::EntityId entity_id) {
  return Blunder::ObjectDB::findByEntityId(entity_id);
}

size_t countObjectsByEntityId(Blunder::EntityId entity_id) {
  struct CountCtx {
    Blunder::EntityId entity_id{Blunder::k_invalid_entity_id};
    size_t count{0};
  };
  CountCtx ctx;
  ctx.entity_id = entity_id;
  Blunder::ObjectDB::forEach(
      [](Blunder::Object* object, void* user) {
        auto* c = static_cast<CountCtx*>(user);
        if (object != nullptr && object->getEntityId() == c->entity_id) {
          ++c->count;
        }
      },
      &ctx);
  return ctx.count;
}

size_t countOccupiedObjects() {
  size_t count = 0;
  Blunder::ObjectDB::forEach(
      [](Blunder::Object*, void* user) {
        ++(*static_cast<size_t*>(user));
      },
      &count);
  return count;
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

/// Re-instantiate must destroy prior bound Objects so findByEntityId does not
/// return a stale Object still holding the old EntityId.
void reinstantiateDestroysStaleBoundObjects() {
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
    }
  ]
}
)";

  Scene scene;
  expect_true("reinst deserialize",
              SceneSerializer::deserialize(eastl::string(kJson), scene));

  SceneInstance instance;
  instance.instantiate(scene);

  const EntityId actor_id_1 = instance.findEntityByName("Actor");
  expect_true("reinst first actor id", isValid(actor_id_1));
  Object* first = ObjectDB::findByEntityId(actor_id_1);
  expect_true("reinst first Object bound", first != nullptr);
  const ObjectId first_id =
      first != nullptr ? first->getId() : k_invalid_object_id;
  expect_true("reinst one occupied Object after first instantiate",
              countOccupiedObjects() == 1);

  instance.clear();
  expect_true("reinst clear destroys bound Object",
              countOccupiedObjects() == 0);
  expect_true("reinst findByEntityId empty after clear",
              ObjectDB::findByEntityId(actor_id_1) == nullptr);
  expect_true("reinst first ObjectId invalidated after clear",
              ObjectDB::get(first_id) == nullptr);

  instance.instantiate(scene);
  const EntityId actor_id_2 = instance.findEntityByName("Actor");
  expect_true("reinst second actor id", isValid(actor_id_2));
  expect_true("reinst EntityId stable across reinstantiate",
              actor_id_2 == actor_id_1);
  expect_true("reinst exactly one Object for EntityId",
              countObjectsByEntityId(actor_id_2) == 1);
  expect_true("reinst one occupied Object after second instantiate",
              countOccupiedObjects() == 1);

  Object* second = ObjectDB::findByEntityId(actor_id_2);
  expect_true("reinst second Object bound", second != nullptr);
  if (second != nullptr) {
    expect_true("reinst second ObjectId differs from destroyed first",
                second->getId() != first_id);
    expect_true("reinst second Object entity id",
                second->getEntityId() == actor_id_2);
    expect_true("reinst two behaviour slots",
                second->getBehaviourCount() == 2);
    expect_true("reinst slot 0 id 1",
                second->getBehaviourIdAt(0) == static_cast<BehaviourId>(1));
    expect_true("reinst slot 1 id 3",
                second->getBehaviourIdAt(1) == static_cast<BehaviourId>(3));
  }

  // instantiate() calls clear() first — a third pass must not accumulate.
  instance.instantiate(scene);
  expect_true("reinst third pass still one Object",
              countOccupiedObjects() == 1);
  expect_true("reinst third pass one binding",
              countObjectsByEntityId(instance.findEntityByName("Actor")) == 1);

  ObjectDB::clear();
}

}  // namespace

int main() {
  instantiateRestoresBehaviourSlotsWithoutHost();
  reinstantiateDestroysStaleBoundObjects();

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
