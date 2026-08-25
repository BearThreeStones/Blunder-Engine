#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/hierarchy_create_ops.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/scene_instance.h"

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

}  // namespace

int main() {
  using namespace Blunder;

  {
    expect_true("null scene no-op",
                !applyHierarchyCreate(nullptr, k_invalid_entity_id,
                                      HierarchyCreateKind::empty)
                     .created);
  }

  {
    SceneInstance scene;
    const EntityId parent = scene.createEntity(
        "A", Vec3(2.0f, 3.0f, 4.0f), glm::identity<Quat>(), Vec3(2.0f));
    const HierarchyCreateResult result =
        applyHierarchyCreate(&scene, parent, HierarchyCreateKind::empty);
    expect_true("empty created", result.created);
    const Entity* child = scene.getEntity(result.entity_id);
    expect_true("empty entity", child != nullptr);
    expect_true("empty parent A",
                child != nullptr && child->getParentId() == parent);
    expect_true("empty name", child != nullptr && child->getName() == "Empty");
    expect_true("empty no camera", scene.getCamera(result.entity_id) == nullptr);
    expect_true("empty no light", scene.getLight(result.entity_id) == nullptr);
    expect_true("parent has no light", scene.getLight(parent) == nullptr);
  }

  {
    SceneInstance scene;
    const HierarchyCreateResult result = applyHierarchyCreate(
        &scene, k_invalid_entity_id, HierarchyCreateKind::camera);
    expect_true("root camera created", result.created);
    const Entity* entity = scene.getEntity(result.entity_id);
    expect_true("root camera no parent",
                entity != nullptr && !isValid(entity->getParentId()));
    expect_true("root camera name",
                entity != nullptr && entity->getName() == "Camera");
    const CameraComponent* camera = scene.getCamera(result.entity_id);
    expect_true("has camera", camera != nullptr);
    expect_true("camera not main", camera != nullptr && !camera->is_main);
  }

  {
    SceneInstance scene;
    const EntityId main_cam = scene.createEntity(
        "Main Camera", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    CameraComponent main{};
    main.is_main = true;
    scene.setCamera(main_cam, main);

    const HierarchyCreateResult result = applyHierarchyCreate(
        &scene, k_invalid_entity_id, HierarchyCreateKind::camera);
    expect_true("second camera created", result.created);
    const CameraComponent* created = scene.getCamera(result.entity_id);
    expect_true("new camera not main", created != nullptr && !created->is_main);
    const CameraComponent* original = scene.getCamera(main_cam);
    expect_true("existing main stays", original != nullptr && original->is_main);
    expect_true("new camera named Camera",
                scene.getEntity(result.entity_id) != nullptr &&
                    scene.getEntity(result.entity_id)->getName() == "Camera");
  }

  {
    SceneInstance scene;
    const EntityId parent = scene.createEntity(
        "Host", Vec3(5.0f, 6.0f, 7.0f), glm::identity<Quat>(), Vec3(3.0f));
    const HierarchyCreateResult result =
        applyHierarchyCreate(&scene, parent, HierarchyCreateKind::light);
    expect_true("light created", result.created);
    expect_true("parent still no light", scene.getLight(parent) == nullptr);
    const LightComponent* light = scene.getLight(result.entity_id);
    expect_true("child has light", light != nullptr);
    expect_true("light directional",
                light != nullptr && light->type == LightType::directional);
    const Entity* child = scene.getEntity(result.entity_id);
    expect_true("light identity pos",
                child != nullptr && child->getPosition() == Vec3(0.0f));
    expect_true("light identity rot",
                child != nullptr && child->getRotation() == glm::identity<Quat>());
    expect_true("light identity scale",
                child != nullptr && child->getScale() == Vec3(1.0f));
    expect_true("light name", child != nullptr && child->getName() == "Light");
  }

  {
    SceneInstance scene;
    scene.createEntity("Light", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const HierarchyCreateResult result = applyHierarchyCreate(
        &scene, k_invalid_entity_id, HierarchyCreateKind::light);
    expect_true("collision created", result.created);
    expect_true("Light_1",
                scene.getEntity(result.entity_id) != nullptr &&
                    scene.getEntity(result.entity_id)->getName() == "Light_1");
  }

  {
    SceneInstance scene;
    const EntityId first = scene.createEntity(
        "Light", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    scene.softDeleteEntity(first);
    const HierarchyCreateResult result = applyHierarchyCreate(
        &scene, k_invalid_entity_id, HierarchyCreateKind::light);
    expect_true("reuse tombstoned name",
                result.created && scene.getEntity(result.entity_id) != nullptr &&
                    scene.getEntity(result.entity_id)->getName() == "Light");
  }

  {
    SceneInstance scene;
    const EntityId parent = scene.createEntity(
        "A", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    const HierarchyCreateResult result =
        applyHierarchyCreate(&scene, parent, HierarchyCreateKind::light);
    expect_true("history light created", result.created);

    EntityId restored = k_invalid_entity_id;
    DocumentHistory history;
    history.setSelectionRestorer(
        [&](const SelectionSnapshot& snapshot) { restored = snapshot.primary; });
    history.push(makeSpawnEntityCommand(
        &scene, result.entity_id, SelectionSnapshot{parent},
        SelectionSnapshot{result.entity_id},
        hierarchyCreateCommandLabel(scene.getEntity(result.entity_id)->getName())));

    expect_true("label Create Light",
                history.commandAt(0) != nullptr &&
                    history.commandAt(0)->label() == "Create Light");
    expect_true("undo create light", history.undo());
    expect_true("light tombstoned", scene.isTombstoned(result.entity_id));
    expect_true("undo selection parent", restored == parent);
    expect_true("one command", history.commandCount() == 1);
    expect_true("redo create light", history.redo());
    expect_true("same id restored", !scene.isTombstoned(result.entity_id));
    expect_true("redo has light", scene.getLight(result.entity_id) != nullptr);
    expect_true("redo selection new", restored == result.entity_id);
  }

  {
    SceneInstance scene;
    const HierarchyCreateResult result = applyHierarchyCreate(
        &scene, k_invalid_entity_id, HierarchyCreateKind::empty);
    DocumentHistory history;
    history.push(makeSpawnEntityCommand(
        &scene, result.entity_id, SelectionSnapshot{},
        SelectionSnapshot{result.entity_id},
        hierarchyCreateCommandLabel("Empty")));
    expect_true("undo empty", history.undo());
    expect_true("empty tombstoned", scene.isTombstoned(result.entity_id));
    expect_true("empty one command", history.commandCount() == 1);
    expect_true("empty can redo", history.canRedo());
  }

  {
    SceneInstance scene;
    const EntityId parent = scene.createEntity(
        "P", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    HierarchySystem hierarchy;
    hierarchy.rebuildVisibleTree(&scene);
    expect_true("leaf not auto-expanded", !hierarchy.isExpanded(parent));
    hierarchy.ensureExpanded(parent);
    expect_true("ensure expanded", hierarchy.isExpanded(parent));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
