#include "runtime/function/editor/editor_commands.h"

#include "runtime/core/object/object.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {
namespace {

void applyBehaviourProperty(Object* object, BehaviourId behaviour_id,
                            const eastl::string& key, const Variant& value) {
  if (object == nullptr) {
    return;
  }
  eastl::vector<SceneBehaviourProperty> bag;
  if (const eastl::vector<SceneBehaviourProperty>* existing =
          object->getBehaviourProperties(behaviour_id);
      existing != nullptr) {
    bag = *existing;
  }

  bool found = false;
  for (SceneBehaviourProperty& prop : bag) {
    if (prop.key == key) {
      prop.value = value;
      found = true;
      break;
    }
  }
  if (!found) {
    SceneBehaviourProperty prop;
    prop.key = key;
    prop.value = value;
    bag.push_back(eastl::move(prop));
  }
  object->setBehaviourProperties(behaviour_id, eastl::move(bag));
}

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

class AddBehaviourCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{k_invalid_entity_id};
  eastl::string clr_type;
  BehaviourId created_id{k_invalid_behaviour_id};

  void redo() override {
    Object* object = scene->ensureBoundObject(entity_id);
    if (object == nullptr) {
      return;
    }
    if (!isValidBehaviourId(created_id)) {
      created_id = object->addBehaviour(clr_type);
    } else {
      object->restoreBehaviour(created_id, clr_type);
    }
  }

  void undo() override {
    Object* object = scene->findBoundObject(entity_id);
    if (object != nullptr) {
      object->removeBehaviour(created_id);
    }
  }
};

class RemoveBehaviourCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{k_invalid_entity_id};
  BehaviourId behaviour_id{k_invalid_behaviour_id};
  size_t index_at_remove{0};
  eastl::string type_name;
  eastl::vector<SceneBehaviourProperty> properties;

  void undo() override {
    Object* object = scene->ensureBoundObject(entity_id);
    if (object == nullptr) {
      return;
    }
    if (!object->restoreBehaviour(behaviour_id, type_name)) {
      return;
    }
    object->setBehaviourProperties(behaviour_id, properties);
    const size_t count = object->getBehaviourCount();
    if (count == 0) {
      return;
    }
    const size_t current_index = count - 1;
    if (current_index != index_at_remove) {
      object->moveBehaviour(current_index, index_at_remove);
    }
  }

  void redo() override {
    Object* object = scene->findBoundObject(entity_id);
    if (object != nullptr) {
      object->removeBehaviour(behaviour_id);
    }
  }
};

class ReorderBehavioursCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{k_invalid_entity_id};
  size_t from_index{0};
  size_t to_index{0};

  void undo() override {
    Object* object = scene->findBoundObject(entity_id);
    if (object == nullptr) {
      return;
    }
    const size_t current_index =
        from_index < to_index ? to_index - 1 : to_index;
    object->moveBehaviour(current_index, from_index);
  }

  void redo() override {
    Object* object = scene->findBoundObject(entity_id);
    if (object != nullptr) {
      object->moveBehaviour(from_index, to_index);
    }
  }
};

class SetBehaviourPropertyCommand final : public IEditorCommand {
 public:
  SceneInstance* scene{nullptr};
  EntityId entity_id{k_invalid_entity_id};
  BehaviourId behaviour_id{k_invalid_behaviour_id};
  eastl::string key;
  Variant before_value;
  Variant after_value;

  void undo() override { apply(before_value); }

  void redo() override { apply(after_value); }

 private:
  void apply(const Variant& value) {
    Object* object = scene->findBoundObject(entity_id);
    if (object == nullptr) {
      return;
    }
    applyBehaviourProperty(object, behaviour_id, key, value);
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

eastl::unique_ptr<IEditorCommand> makeAddBehaviourCommand(
    SceneInstance* scene, EntityId entity_id, const eastl::string& clr_type,
    BehaviourId created_id, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after) {
  auto command = eastl::make_unique<AddBehaviourCommand>();
  command->scene = scene;
  command->entity_id = entity_id;
  command->clr_type = clr_type;
  command->created_id = created_id;
  command->selection_before = selection_before;
  command->selection_after = selection_after;
  return command;
}

eastl::unique_ptr<IEditorCommand> makeRemoveBehaviourCommand(
    SceneInstance* scene, EntityId entity_id, BehaviourId behaviour_id,
    size_t index_at_remove, const eastl::string& type_name,
    eastl::vector<SceneBehaviourProperty> properties,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after) {
  auto command = eastl::make_unique<RemoveBehaviourCommand>();
  command->scene = scene;
  command->entity_id = entity_id;
  command->behaviour_id = behaviour_id;
  command->index_at_remove = index_at_remove;
  command->type_name = type_name;
  command->properties = eastl::move(properties);
  command->selection_before = selection_before;
  command->selection_after = selection_after;
  return command;
}

eastl::unique_ptr<IEditorCommand> makeReorderBehavioursCommand(
    SceneInstance* scene, EntityId entity_id, size_t from_index,
    size_t to_index, SelectionSnapshot selection_before,
    SelectionSnapshot selection_after) {
  auto command = eastl::make_unique<ReorderBehavioursCommand>();
  command->scene = scene;
  command->entity_id = entity_id;
  command->from_index = from_index;
  command->to_index = to_index;
  command->selection_before = selection_before;
  command->selection_after = selection_after;
  return command;
}

eastl::unique_ptr<IEditorCommand> makeSetBehaviourPropertyCommand(
    SceneInstance* scene, EntityId entity_id, BehaviourId behaviour_id,
    const eastl::string& key, Variant before_value, Variant after_value,
    SelectionSnapshot selection_before, SelectionSnapshot selection_after) {
  auto command = eastl::make_unique<SetBehaviourPropertyCommand>();
  command->scene = scene;
  command->entity_id = entity_id;
  command->behaviour_id = behaviour_id;
  command->key = key;
  command->before_value = eastl::move(before_value);
  command->after_value = eastl::move(after_value);
  command->selection_before = selection_before;
  command->selection_after = selection_after;
  return command;
}

}  // namespace Blunder
