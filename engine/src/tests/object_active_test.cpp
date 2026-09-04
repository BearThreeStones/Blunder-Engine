#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/editor/object_active_ops.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"

#include <glm/gtc/quaternion.hpp>

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

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    expect_true("default Object Active on", scene.isObjectActive(id));
    expect_true("default Active in Hierarchy", scene.isActiveInHierarchy(id));
    scene.setObjectActive(id, false);
    expect_true("local off", !scene.isObjectActive(id));
    expect_true("hierarchy off when local off", !scene.isActiveInHierarchy(id));
    scene.setObjectActive(id, true);
    expect_true("local restored", scene.isObjectActive(id));
  }

  {
    SceneInstance scene;
    const EntityId parent =
        scene.createEntity("P", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId child = scene.createEntity(
        "C", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1), parent);
    expect_true("child starts on", scene.isObjectActive(child));
    scene.setObjectActive(parent, false);
    expect_true("parent off keeps child flag", scene.isObjectActive(child));
    expect_true("child not Active in Hierarchy",
                !scene.isActiveInHierarchy(child));
    expect_true("parent not Active in Hierarchy",
                !scene.isActiveInHierarchy(parent));
    scene.setObjectActive(parent, true);
    expect_true("restoring parent restores hierarchy without rewriting child",
                scene.isObjectActive(child) && scene.isActiveInHierarchy(child));
    scene.setObjectActive(child, false);
    scene.setObjectActive(parent, false);
    scene.setObjectActive(parent, true);
    expect_true("inactive child stays off after parent toggle",
                !scene.isObjectActive(child) &&
                    !scene.isActiveInHierarchy(child));
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Tomb", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    expect_true("soft-delete", scene.softDeleteEntity(id));
    expect_true("tombstone not Active in Hierarchy",
                !scene.isActiveInHierarchy(id));
  }

  {
    SceneInstance scene;
    const EntityId a =
        scene.createEntity("A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId b =
        scene.createEntity("B", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    eastl::vector<EntityId> both{a, b};
    expect_true("all on -> align off", !alignedObjectActiveAfter(scene, both));
    scene.setObjectActive(b, false);
    expect_true("mixed -> align on", alignedObjectActiveAfter(scene, both));
    scene.setObjectActive(a, false);
    expect_true("all off -> align on", alignedObjectActiveAfter(scene, both));
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Named", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    scene.setObjectActive(id, false);
    eastl::vector<ObjectActiveEntry> entries;
    ObjectActiveEntry entry;
    entry.entity_id = id;
    entry.before = true;
    entry.after = false;
    entries.push_back(entry);

    DocumentHistory history;
    history.push(makeSetObjectActiveCommand(&scene, eastl::move(entries),
                                            SelectionSnapshot{id},
                                            SelectionSnapshot{id}));
    expect_true("command is v1 patchable",
                history.commandAt(0) != nullptr &&
                    history.commandAt(0)->isPlayV1Patchable());
    expect_true("undo restores Active", history.undo() && scene.isObjectActive(id));
    expect_true("redo applies inactive",
                history.redo() && !scene.isObjectActive(id));
  }

  {
    SceneInstance scene;
    const EntityId a =
        scene.createEntity("A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId b =
        scene.createEntity("B", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.setObjectActive(b, false);
    eastl::vector<ObjectActiveEntry> entries;
    ObjectActiveEntry ea;
    ea.entity_id = a;
    ea.before = true;
    ea.after = true;
    ObjectActiveEntry eb;
    eb.entity_id = b;
    eb.before = false;
    eb.after = true;
    entries.push_back(ea);
    entries.push_back(eb);
    scene.setObjectActive(a, true);
    scene.setObjectActive(b, true);
    DocumentHistory history;
    auto command = makeSetObjectActiveCommand(&scene, eastl::move(entries),
                                              SelectionSnapshot{a},
                                              SelectionSnapshot{a});
    expect_true("multi-select ids on command",
                command->play_v1_entity_ids.size() == 2);
    history.push(eastl::move(command));
    expect_true("undo mixed restore", history.undo() && scene.isObjectActive(a) &&
                                          !scene.isObjectActive(b));
  }

  {
    SceneInstance live;
    const EntityId parent = live.createEntity(
        "Parent", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId child = live.createEntity(
        "Child", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1), parent);
    live.setObjectActive(parent, false);

    Scene exported;
    expect_true("export", live.exportToScene(exported));
    eastl::string json;
    expect_true("serialize", SceneSerializer::serialize(exported, json));
    expect_true("writes parent active false",
                json.find("\"active\": false") != eastl::string::npos);
    const size_t child_pos = json.find("\"name\": \"Child\"");
    expect_true("child in json", child_pos != eastl::string::npos);
    if (child_pos != eastl::string::npos) {
      const size_t next_entity = json.find("\"name\":", child_pos + 1);
      const eastl::string child_slice =
          next_entity == eastl::string::npos
              ? json.substr(child_pos)
              : json.substr(child_pos, next_entity - child_pos);
      expect_true("omits child active when on",
                  child_slice.find("\"active\"") == eastl::string::npos);
    }

    Scene loaded;
    expect_true("deserialize", SceneSerializer::deserialize(json, loaded));
    SceneInstance round_trip;
    round_trip.instantiate(loaded);
    const EntityId loaded_parent = round_trip.findEntityByName("Parent");
    const EntityId loaded_child = round_trip.findEntityByName("Child");
    expect_true("loaded parent inactive",
                isValid(loaded_parent) &&
                    !round_trip.isObjectActive(loaded_parent));
    expect_true("loaded child flag on",
                isValid(loaded_child) && round_trip.isObjectActive(loaded_child));
    expect_true("loaded child not Active in Hierarchy",
                isValid(loaded_child) &&
                    !round_trip.isActiveInHierarchy(loaded_child));
  }

  {
    const char* missing_active =
        "{\n"
        "  \"type\": \"Scene\",\n"
        "  \"entities\": [\n"
        "    {\n"
        "      \"name\": \"Legacy\",\n"
        "      \"position\": [0, 0, 0],\n"
        "      \"rotation\": [0, 0, 0],\n"
        "      \"rotationMode\": \"euler_degrees\"\n"
        "    }\n"
        "  ]\n"
        "}\n";
    Scene loaded;
    expect_true("deserialize missing active",
                SceneSerializer::deserialize(eastl::string(missing_active),
                                             loaded));
    expect_true("missing active means on",
                loaded.getEntities().size() == 1 &&
                    loaded.getEntities()[0].active);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Cached", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    HierarchySystem hierarchy;
    hierarchy.rebuildVisibleTree(&scene);
    expect_true("tree row starts active",
                !hierarchy.treeRows().empty() &&
                    hierarchy.treeRows()[0].object_active &&
                    hierarchy.treeRows()[0].active_in_hierarchy);
    scene.setObjectActive(id, false);
    expect_true("tree rows stay stale until rebuild",
                !hierarchy.treeRows().empty() &&
                    hierarchy.treeRows()[0].object_active);
    hierarchy.rebuildVisibleTree(&scene);
    expect_true("rebuild copies Object Active and Active in Hierarchy",
                !hierarchy.treeRows().empty() &&
                    !hierarchy.treeRows()[0].object_active &&
                    !hierarchy.treeRows()[0].active_in_hierarchy);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    g_runtime_global_context.m_logger_system.reset();
    return 1;
  }
  std::fprintf(stderr, "object_active_test: all passed\n");
  g_runtime_global_context.m_logger_system.reset();
  return 0;
}
