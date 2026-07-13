#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/scene/scene_instance.h"

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

  // Transform command round-trip
  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("T", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    Entity* entity = scene.getEntity(id);
    expect_true("entity", entity != nullptr);

    const Vec3 before = entity->getPosition();
    entity->setPosition(Vec3(5, 0, 0));
    const Vec3 after = entity->getPosition();

    DocumentHistory history;
    history.push(makeSetEntityTransformCommand(
        &scene, id, before, entity->getRotation(), entity->getScale(), after,
        entity->getRotation(), entity->getScale(), SelectionSnapshot{id},
        SelectionSnapshot{id}));

    expect_true("undo transform", history.undo());
    expect_true("pos restored",
                scene.getEntity(id)->getPosition() == before);
    expect_true("redo transform", history.redo());
    expect_true("pos reapplied",
                scene.getEntity(id)->getPosition() == after);
  }

  // Soft-delete command
  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("D", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.softDeleteEntity(id);

    DocumentHistory history;
    history.push(makeSoftDeleteEntityCommand(&scene, id, SelectionSnapshot{id},
                                             SelectionSnapshot{}));

    expect_true("undo soft delete", history.undo());
    expect_true("restored", !scene.isTombstoned(id));
    expect_true("redo soft delete", history.redo());
    expect_true("tombstoned again", scene.isTombstoned(id));
  }

  // Spawn command: create then undo soft-deletes
  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("S", Vec3(1, 2, 3), glm::identity<Quat>(), Vec3(1));

    DocumentHistory history;
    history.push(makeSpawnEntityCommand(&scene, id, SelectionSnapshot{},
                                        SelectionSnapshot{id}));

    expect_true("undo spawn", history.undo());
    expect_true("spawn tombstoned", scene.isTombstoned(id));
    Scene exported;
    scene.exportToScene(exported);
    expect_true("export empty after undo spawn",
                exported.getEntities().empty());
    expect_true("redo spawn", history.redo());
    expect_true("spawn restored", !scene.isTombstoned(id));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
