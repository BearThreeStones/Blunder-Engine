#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/project/authorship_issue.h"
#include "runtime/project/play_authorship_patch.h"
#include "runtime/function/scene/scene_instance.h"

#include <cstdio>
#include <string>

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

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Hero", Vec3(1, 2, 3), glm::identity<Quat>(),
                           Vec3(1, 1, 1));
    const std::string json = buildPlayAuthorshipPatchJson(scene, id);
    expect_true("snapshot has address",
                json.find("\"address\":\"Hero\"") != std::string::npos);

    SceneInstance other;
    const EntityId other_id =
        other.createEntity("Hero", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1, 1, 1));
    std::string unknown;
    expect_true("apply known address",
                applyPlayAuthorshipPatchJson(other, json, &unknown));
    expect_true("unknown empty", unknown.empty());
    expect_true("trs copied",
                other.getEntity(other_id)->getPosition() == Vec3(1, 2, 3));
  }

  {
    SceneInstance scene;
    (void)scene.createEntity("Hero", Vec3(0, 0, 0), glm::identity<Quat>(),
                             Vec3(1));
    std::string unknown;
    expect_true(
        "unknown address fails",
        !applyPlayAuthorshipPatchJson(
            scene, "{\"address\":\"Ghost\",\"local\":{\"t\":[1,0,0]}}",
            &unknown));
    expect_true("unknown address name", unknown == "Ghost");
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("T", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    auto transform = makeSetEntityTransformCommand(
        &scene, id, Vec3(0), glm::identity<Quat>(), Vec3(1), Vec3(5, 0, 0),
        glm::identity<Quat>(), Vec3(1), SelectionSnapshot{id},
        SelectionSnapshot{id});
    expect_true("transform is v1", transform->isPlayV1Patchable());
    auto spawn = makeSpawnEntityCommand(&scene, id, SelectionSnapshot{},
                                        SelectionSnapshot{id});
    expect_true("spawn is not v1", !spawn->isPlayV1Patchable());
    auto del = makeSoftDeleteEntityCommand(&scene, id, SelectionSnapshot{id},
                                           SelectionSnapshot{});
    expect_true("delete is not v1", !del->isPlayV1Patchable());
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("T", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    Entity* entity = scene.getEntity(id);
    const Vec3 before = entity->getPosition();
    entity->setPosition(Vec3(5, 0, 0));
    const Vec3 after = entity->getPosition();

    int v1_fires = 0;
    DocumentHistory history;
    history.setAfterMutationObserver([&](const IEditorCommand& command) {
      if (command.isPlayV1Patchable()) {
        ++v1_fires;
      }
    });
    history.push(makeSetEntityTransformCommand(
        &scene, id, before, entity->getRotation(), entity->getScale(), after,
        entity->getRotation(), entity->getScale(), SelectionSnapshot{id},
        SelectionSnapshot{id}));
    expect_true("push fires v1", v1_fires == 1);
    expect_true("undo fires v1", history.undo());
    expect_true("undo restored", entity->getPosition() == before);
    expect_true("undo fire count", v1_fires == 2);
    expect_true("redo fires v1", history.redo());
    expect_true("redo restored", entity->getPosition() == after);
    expect_true("redo fire count", v1_fires == 3);
    expect_true("jump fires v1", history.jumpTo(0));
    expect_true("jump fire count", v1_fires == 4);

    history.push(makeSpawnEntityCommand(&scene, id, SelectionSnapshot{},
                                        SelectionSnapshot{id}));
    expect_true("spawn mutation does not count v1", v1_fires == 4);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Hero", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    scene.setObjectActive(id, false);
    const std::string json = buildPlayAuthorshipPatchJson(scene, id);
    expect_true("snapshot has active false",
                json.find("\"active\":false") != std::string::npos);

    SceneInstance other;
    const EntityId other_id =
        other.createEntity("Hero", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    expect_true("other starts active", other.isObjectActive(other_id));
    std::string unknown;
    expect_true("apply active patch",
                applyPlayAuthorshipPatchJson(other, json, &unknown));
    expect_true("active copied off", !other.isObjectActive(other_id));
  }

  {
    SceneInstance scene;
    const EntityId a =
        scene.createEntity("A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId b =
        scene.createEntity("B", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    eastl::vector<ObjectActiveEntry> entries;
    ObjectActiveEntry ea;
    ea.entity_id = a;
    ea.before = true;
    ea.after = false;
    ObjectActiveEntry eb;
    eb.entity_id = b;
    eb.before = true;
    eb.after = false;
    entries.push_back(ea);
    entries.push_back(eb);
    auto command = makeSetObjectActiveCommand(
        &scene, eastl::move(entries), SelectionSnapshot{a},
        SelectionSnapshot{a});
    expect_true("active command is v1", command->isPlayV1Patchable());
    expect_true("active command lists both ids",
                command->play_v1_entity_ids.size() == 2);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "play_authorship_patch_test: all passed\n");
  return 0;
}
