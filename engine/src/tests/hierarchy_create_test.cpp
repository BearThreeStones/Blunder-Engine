#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/hierarchy_create_ops.h"
#include "runtime/function/editor/hierarchy_system.h"
#include "runtime/core/object/object.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/scene_instance.h"

#include <glm/gtc/quaternion.hpp>

#include <chrono>
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

  {
    SceneInstance scene;
    const EntityId root_a = scene.createEntity(
        "A", Vec3(0), glm::identity<Quat>(), Vec3(1));
    const EntityId root_b = scene.createEntity(
        "B", Vec3(0), glm::identity<Quat>(), Vec3(1));
    const EntityId deleted_root = scene.createEntity(
        "Deleted", Vec3(0), glm::identity<Quat>(), Vec3(1));
    const EntityId child_a1 = scene.createEntity(
        "A1", Vec3(0), glm::identity<Quat>(), Vec3(1), root_a);
    const EntityId child_a2 = scene.createEntity(
        "A2", Vec3(0), glm::identity<Quat>(), Vec3(1), root_a);
    const EntityId grandchild = scene.createEntity(
        "A1a", Vec3(0), glm::identity<Quat>(), Vec3(1), child_a1);
    CameraComponent camera{};
    scene.setCamera(child_a2, camera);
    scene.softDeleteEntity(deleted_root);

    HierarchySystem hierarchy;
    hierarchy.rebuildVisibleTree(&scene);
    const auto& collapsed_rows = hierarchy.treeRows();
    expect_true("nested collapsed row count", collapsed_rows.size() == 4u);
    expect_true("root order A then B",
                collapsed_rows.size() == 4u &&
                    collapsed_rows[0].entity_id == root_a &&
                    collapsed_rows[3].entity_id == root_b);
    expect_true("collapsed child hides grandchild",
                collapsed_rows.size() == 4u &&
                    collapsed_rows[1].entity_id == child_a1 &&
                    collapsed_rows[1].has_children &&
                    !collapsed_rows[1].is_expanded &&
                    collapsed_rows[2].entity_id == child_a2);
    expect_true("tombstoned root omitted",
                collapsed_rows.size() == 4u &&
                    collapsed_rows[0].display_name != "Deleted" &&
                    collapsed_rows[3].display_name != "Deleted");
    expect_true(
        "camera icon preserved",
        collapsed_rows.size() == 4u && collapsed_rows[2].icons.size() == 2u &&
            collapsed_rows[2].icons[0].kind ==
                HierarchyRowIconKind::Transform &&
            collapsed_rows[2].icons[1].kind == HierarchyRowIconKind::Camera);

    hierarchy.ensureExpanded(child_a1);
    hierarchy.rebuildVisibleTree(&scene);
    const auto& expanded_rows = hierarchy.treeRows();
    expect_true("nested expanded row count", expanded_rows.size() == 5u);
    expect_true(
        "nested row metadata preserved",
        expanded_rows.size() == 5u &&
            expanded_rows[0].entity_id == root_a &&
            expanded_rows[0].depth == 0 &&
            !expanded_rows[0].is_last_sibling &&
            expanded_rows[1].entity_id == child_a1 &&
            expanded_rows[1].depth == 1 &&
            !expanded_rows[1].is_last_sibling &&
            expanded_rows[2].entity_id == grandchild &&
            expanded_rows[2].depth == 2 &&
            expanded_rows[2].is_last_sibling &&
            expanded_rows[2].ancestor_cont_mask == 1u &&
            expanded_rows[3].entity_id == child_a2 &&
            expanded_rows[3].depth == 1 &&
            expanded_rows[3].is_last_sibling &&
            expanded_rows[4].entity_id == root_b &&
            expanded_rows[4].depth == 0 &&
            expanded_rows[4].is_last_sibling);
  }

  {
    constexpr size_t k_entity_count = 10000u;
    SceneInstance scene;
    for (size_t i = 0; i < k_entity_count; ++i) {
      scene.createEntity("Entity", Vec3(0), glm::identity<Quat>(), Vec3(1));
    }

    HierarchySystem hierarchy;
    const auto start = std::chrono::steady_clock::now();
    hierarchy.rebuildVisibleTree(&scene);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::fprintf(stdout, "Hierarchy 10k rebuild took %lld ms\n",
                 static_cast<long long>(elapsed.count()));
    expect_true("10k hierarchy rebuild under 500 ms",
                hierarchy.treeRows().size() == k_entity_count &&
                    elapsed < std::chrono::milliseconds(500));
  }

  {
    SceneInstance scene;
    const EntityId parent_entity = scene.createEntity(
        "Bound Parent", Vec3(0), glm::identity<Quat>(), Vec3(1));
    const EntityId child_entity = scene.createEntity(
        "Bound Child", Vec3(0), glm::identity<Quat>(), Vec3(1));
    Object* parent = scene.ensureBoundObject(parent_entity);
    Object* child = scene.ensureBoundObject(child_entity);
    expect_true("bound parent and child", parent != nullptr && child != nullptr);
    const ObjectId parent_object_id =
        parent != nullptr ? parent->getId() : k_invalid_object_id;
    const ObjectId child_object_id =
        child != nullptr ? child->getId() : k_invalid_object_id;
    if (parent != nullptr && child != nullptr) {
      child->setParent(parent);
    }

    scene.releaseBoundObject(parent_entity);
    Object* surviving_child = scene.findBoundObject(child_entity);
    expect_true("released parent binding removed",
                scene.findBoundObject(parent_entity) == nullptr);
    expect_true("bound child survives parent release",
                surviving_child != nullptr &&
                    surviving_child->getId() == child_object_id &&
                    !isValid(surviving_child->getParentId()));

    Object* rebound_parent = scene.ensureBoundObject(parent_entity);
    expect_true("released slot rebinds with new generation",
                rebound_parent != nullptr &&
                    rebound_parent->getId() != parent_object_id);
    expect_true("child lookup remains stable after slot reuse",
                surviving_child != nullptr &&
                    scene.findBoundObject(child_entity) == surviving_child);
  }

  {
    constexpr size_t k_entity_count = 10000u;
    SceneInstance scene;
    for (size_t i = 0; i < k_entity_count; ++i) {
      scene.createEntity("Bound Entity", Vec3(0), glm::identity<Quat>(), Vec3(1));
    }
    bool all_objects_bound = true;
    for (size_t i = 0; i < k_entity_count; ++i) {
      all_objects_bound &=
          scene.ensureBoundObject(scene.getEntityIdAtIndex(i)) != nullptr;
    }
    expect_true("10k hierarchy objects bound", all_objects_bound);

    HierarchySystem hierarchy;
    const auto start = std::chrono::steady_clock::now();
    hierarchy.rebuildVisibleTree(&scene);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    std::fprintf(stdout, "Hierarchy 10k bound-object rebuild took %lld ms\n",
                 static_cast<long long>(elapsed.count()));
    expect_true("10k bound-object hierarchy rebuild under 500 ms",
                hierarchy.treeRows().size() == k_entity_count &&
                    elapsed < std::chrono::milliseconds(500));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
