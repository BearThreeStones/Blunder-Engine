#include "runtime/function/editor/editor_commands.h"

#include "runtime/function/scene/scene_instance.h"

namespace Blunder {
namespace {

class SetEntityTransformCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{k_invalid_entity_id};
  Vec3 before_position{};
  Quat before_rotation{glm::identity<Quat>()};
  Vec3 before_scale{1.0f};
  Vec3 after_position{};
  Quat after_rotation{glm::identity<Quat>()};
  Vec3 after_scale{1.0f};

  void undo() override { apply(before_position, before_rotation, before_scale); }

  void redo() override { apply(after_position, after_rotation, after_scale); }

 private:
  void apply(const Vec3& position, const Quat& rotation, const Vec3& scale) {
    if (scene == nullptr) {
      return;
    }
    Entity* entity = scene->getEntity(entity_id);
    if (entity == nullptr) {
      return;
    }
    entity->setPosition(position);
    entity->setRotation(rotation);
    entity->setScale(scale);
    scene->markTransformsDirty();
  }
};

class SoftDeleteEntityCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{k_invalid_entity_id};

  void undo() override {
    if (scene != nullptr) {
      scene->restoreEntity(entity_id);
    }
  }

  void redo() override {
    if (scene != nullptr) {
      scene->softDeleteEntity(entity_id);
    }
  }
};

class SpawnEntityCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{k_invalid_entity_id};

  void undo() override {
    if (scene != nullptr) {
      scene->softDeleteEntity(entity_id);
    }
  }

  void redo() override {
    if (scene != nullptr) {
      scene->restoreEntity(entity_id);
    }
  }
};

}  // namespace

eastl::unique_ptr<IEditorCommand> makeSetEntityTransformCommand(
    SceneInstance* scene, EntityId entity_id, const Vec3& before_position,
    const Quat& before_rotation, const Vec3& before_scale,
    const Vec3& after_position, const Quat& after_rotation,
    const Vec3& after_scale, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after) {
  auto command = eastl::make_unique<SetEntityTransformCommand>();
  command->scene = scene;
  command->entity_id = entity_id;
  command->before_position = before_position;
  command->before_rotation = before_rotation;
  command->before_scale = before_scale;
  command->after_position = after_position;
  command->after_rotation = after_rotation;
  command->after_scale = after_scale;
  command->selection_before = selection_before;
  command->selection_after = selection_after;
  return command;
}

eastl::unique_ptr<IEditorCommand> makeSoftDeleteEntityCommand(
    SceneInstance* scene, EntityId entity_id,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after) {
  auto command = eastl::make_unique<SoftDeleteEntityCommand>();
  command->scene = scene;
  command->entity_id = entity_id;
  command->selection_before = selection_before;
  command->selection_after = selection_after;
  return command;
}

eastl::unique_ptr<IEditorCommand> makeSpawnEntityCommand(
    SceneInstance* scene, EntityId entity_id,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after) {
  auto command = eastl::make_unique<SpawnEntityCommand>();
  command->scene = scene;
  command->entity_id = entity_id;
  command->selection_before = selection_before;
  command->selection_after = selection_after;
  return command;
}

}  // namespace Blunder
