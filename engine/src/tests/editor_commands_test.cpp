#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/scene.h"
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

    expect_true("default label Delete Entity",
                history.commandAt(0) != nullptr &&
                    history.commandAt(0)->label() == "Delete Entity");
    expect_true("undo soft delete", history.undo());
    expect_true("restored", !scene.isTombstoned(id));
    expect_true("redo soft delete", history.redo());
    expect_true("tombstoned again", scene.isTombstoned(id));
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Cube", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.softDeleteEntity(id);

    DocumentHistory history;
    history.push(makeSoftDeleteEntityCommand(
        &scene, id, SelectionSnapshot{id}, SelectionSnapshot{},
        deleteEntityCommandLabel("Cube")));
    expect_true("label Delete Cube",
                history.commandAt(0) != nullptr &&
                    history.commandAt(0)->label() == "Delete Cube");
  }

  {
    expect_true("empty name Delete Entity",
                deleteEntityCommandLabel("") == "Delete Entity");
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

  // Camera component command round-trip
  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Cam", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    CameraComponent before{};
    before.vertical_fov_degrees = 45.0f;
    before.near_clip = 0.1f;
    before.far_clip = 100.0f;
    scene.setCamera(id, before);

    CameraComponent after = before;
    after.vertical_fov_degrees = 75.0f;
    after.near_clip = 0.25f;
    after.far_clip = 500.0f;
    scene.setCamera(id, after);

    DocumentHistory history;
    history.push(makeSetCameraComponentCommand(
        &scene, id, before, after, SelectionSnapshot{id}, SelectionSnapshot{id}));

    expect_true("undo camera", history.undo());
    const CameraComponent* restored = scene.getCamera(id);
    expect_true("fov restored",
                restored != nullptr && restored->vertical_fov_degrees == 45.0f);
    expect_true("near restored",
                restored != nullptr && restored->near_clip == 0.1f);
    expect_true("redo camera", history.redo());
    const CameraComponent* reapplied = scene.getCamera(id);
    expect_true("fov reapplied",
                reapplied != nullptr && reapplied->vertical_fov_degrees == 75.0f);
    expect_true("far reapplied",
                reapplied != nullptr && reapplied->far_clip == 500.0f);
  }

  // Align camera to view command round-trip
  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Cam", Vec3(1, 2, 3), glm::identity<Quat>(), Vec3(1));
    CameraComponent before_cam{};
    before_cam.vertical_fov_degrees = 45.0f;
    scene.setCamera(id, before_cam);

    const Vec3 after_pos(4, 5, 6);
    const Quat after_rot = glm::angleAxis(glm::radians(30.0f), Vec3(0, 0, 1));
    CameraComponent after_cam = before_cam;
    after_cam.vertical_fov_degrees = 70.0f;

    Entity* entity = scene.getEntity(id);
    entity->setPosition(after_pos);
    entity->setRotation(after_rot);
    scene.setCamera(id, after_cam);

    DocumentHistory history;
    history.push(makeAlignCameraToViewCommand(
        &scene, id, Vec3(1, 2, 3), glm::identity<Quat>(), Vec3(1), before_cam,
        after_pos, after_rot, Vec3(1), after_cam, SelectionSnapshot{id},
        SelectionSnapshot{id}));

    expect_true("undo align camera", history.undo());
    expect_true("pos restored",
                scene.getEntity(id)->getPosition() == Vec3(1, 2, 3));
    const CameraComponent* restored = scene.getCamera(id);
    expect_true("fov restored",
                restored != nullptr && restored->vertical_fov_degrees == 45.0f);
    expect_true("redo align camera", history.redo());
    expect_true("pos reapplied",
                scene.getEntity(id)->getPosition() == after_pos);
    const CameraComponent* reapplied = scene.getCamera(id);
    expect_true("fov reapplied",
                reapplied != nullptr && reapplied->vertical_fov_degrees == 70.0f);
  }

  {
    SceneInstance scene;
    const EntityId parent = scene.createEntity(
        "Parent", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const EntityId child = scene.createEntity(
        "Child", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1), parent);
    scene.softDeleteEntity(parent);

    DocumentHistory history;
    history.push(makeSoftDeleteEntityCommand(
        &scene, parent, SelectionSnapshot{parent}, SelectionSnapshot{},
        deleteEntityCommandLabel("Parent")));

    expect_true("one delete command", history.commandCount() == 1);
    expect_true("parent omitted", scene.isOmittedFromDocument(parent));
    expect_true("child omitted with parent", scene.isOmittedFromDocument(child));
    expect_true("child not tombstoned", !scene.isTombstoned(child));
    Scene exported;
    expect_true("export ok", scene.exportToScene(exported));
    expect_true("export omits subtree", exported.getEntities().empty());

    expect_true("undo parent delete", history.undo());
    expect_true("parent restored same id", !scene.isTombstoned(parent));
    expect_true("child still parented",
                scene.getEntity(child) != nullptr &&
                    scene.getEntity(child)->getParentId() == parent);
    expect_true("child visible after undo", !scene.isOmittedFromDocument(child));
    expect_true("still one history row", history.commandCount() == 1);
  }

  {
    SceneInstance scene;
    const EntityId id = scene.createEntity(
        "Main Camera", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    CameraComponent camera{};
    camera.is_main = true;
    scene.setCamera(id, camera);
    expect_true("soft-delete main camera", scene.softDeleteEntity(id));
    expect_true("main camera tombstoned", scene.isTombstoned(id));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
