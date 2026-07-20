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

/// After instantiate, exportToScene must write Behaviour type+id from the bound
/// Object (live source), not drop the list. Mutations on the Object must round
/// through export. Soft-deleted entities (and their behaviours) stay omitted.
void exportWritesBehavioursFromBoundObjects() {
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
  expect_true("deserialize",
              SceneSerializer::deserialize(eastl::string(kJson), scene));

  SceneInstance instance;
  instance.instantiate(scene);

  const EntityId actor_id = instance.findEntityByName("Actor");
  expect_true("actor entity", isValid(actor_id));
  Object* actor = ObjectDB::findByEntityId(actor_id);
  expect_true("actor Object bound", actor != nullptr);
  if (actor != nullptr) {
    const BehaviourId added = actor->addBehaviour("Game.AfterExport");
    expect_true("live addBehaviour id 4",
                added == static_cast<BehaviourId>(4));
  }

  Scene exported;
  expect_true("export ok", instance.exportToScene(exported));
  expect_true("export two entities", exported.getEntities().size() == 2);

  const SceneEntityDefinition* actor_def = nullptr;
  const SceneEntityDefinition* prop_def = nullptr;
  for (const SceneEntityDefinition& def : exported.getEntities()) {
    if (def.name == "Actor") {
      actor_def = &def;
    } else if (def.name == "Prop") {
      prop_def = &def;
    }
  }
  expect_true("exported Actor", actor_def != nullptr);
  expect_true("exported Prop", prop_def != nullptr);

  if (actor_def != nullptr) {
    expect_true("Actor behaviours from Object (3 slots)",
                actor_def->behaviours.size() == 3);
    if (actor_def->behaviours.size() == 3) {
      expect_true("behaviour 0 type Motor",
                  actor_def->behaviours[0].type == "Game.Motor");
      expect_true("behaviour 0 id 1",
                  actor_def->behaviours[0].id ==
                      static_cast<BehaviourId>(1));
      expect_true("behaviour 0 empty properties",
                  actor_def->behaviours[0].properties.empty());
      expect_true("behaviour 1 type Bark",
                  actor_def->behaviours[1].type == "Game.Bark");
      expect_true("behaviour 1 id 3",
                  actor_def->behaviours[1].id ==
                      static_cast<BehaviourId>(3));
      expect_true("behaviour 2 type AfterExport",
                  actor_def->behaviours[2].type == "Game.AfterExport");
      expect_true("behaviour 2 id 4",
                  actor_def->behaviours[2].id ==
                      static_cast<BehaviourId>(4));
    }
  }
  if (prop_def != nullptr) {
    expect_true("Prop has no behaviours", prop_def->behaviours.empty());
  }

  // Soft-delete Actor: export must omit the entity and its behaviours.
  expect_true("softDelete Actor", instance.softDeleteEntity(actor_id));
  Scene after_delete;
  expect_true("export after soft-delete",
              instance.exportToScene(after_delete));
  expect_true("tombstone omitted from export",
              after_delete.getEntities().size() == 1);
  if (after_delete.getEntities().size() == 1) {
    expect_true("remaining entity is Prop",
                after_delete.getEntities()[0].name == "Prop");
    expect_true("no Actor behaviours on disk",
                after_delete.getEntities()[0].behaviours.empty());
  }

  ObjectDB::clear();
}

}  // namespace

int main() {
  exportWritesBehavioursFromBoundObjects();

  using namespace Blunder;
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stderr, "scene_behaviour_export_test: all passed\n");
  }
  std::fflush(stderr);
  return exit_code;
}
