#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/inspector_animation_player_ops.h"
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

}  // namespace

/// Inspector clip-map edits must persist through exportToScene and scene JSON
/// round-trip; undo/redo restores prior bindings.
void inspectorClipMapEditPersistsThroughExportAndSerialize() {
  using namespace Blunder;
  ensureLogger();
  ObjectDB::clear();

  const char* kIdleGuid = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
  const char* kWalkGuid = "11111111-2222-4333-8444-555555555555";
  const char* kEditedWalkGuid = "22222222-3333-4444-8555-666666666666";

  Scene scene;
  scene.setGuid("cccccccc-cccc-4ccc-8ccc-cccccccccccc");

  SceneEntityDefinition entity;
  entity.name = "Dog";
  entity.has_skeleton = true;
  entity.animation_player_clips.push_back({"idle", eastl::string(kIdleGuid)});
  entity.animation_player_clips.push_back({"walk", eastl::string(kWalkGuid)});
  scene.getEntities().push_back(eastl::move(entity));

  SceneInstance instance;
  instance.instantiate(scene);

  const EntityId dog_id = instance.findEntityByName("Dog");
  expect_true("dog entity", isValid(dog_id));
  Object* object = instance.findBoundObject(dog_id);
  expect_true("bound object", object != nullptr);
  expect_true("has animation player", object != nullptr && object->hasAnimationPlayer());

  AnimationPlayer* player = object->getAnimationPlayer();
  expect_true("player non-null", player != nullptr);
  if (player == nullptr) {
    return;
  }

  eastl::vector<AnimationPlayer::ClipBinding> before_bindings =
      player->getClipBindings();
  expect_true("two clip bindings loaded", before_bindings.size() == 2);

  eastl::vector<AnimationPlayer::ClipBinding> after_bindings = before_bindings;
  for (AnimationPlayer::ClipBinding& binding : after_bindings) {
    if (binding.name == "walk") {
      binding.guid = kEditedWalkGuid;
      break;
    }
  }
  applyClipBindingsToObject(object, after_bindings);

  eastl::string edited_guid;
  expect_true("edited walk guid readable",
              player->getClipGuid("walk", edited_guid) &&
                  edited_guid == kEditedWalkGuid);

  Scene exported;
  expect_true("export ok", instance.exportToScene(exported));
  expect_true("one exported entity", exported.getEntities().size() == 1);
  if (exported.getEntities().size() == 1) {
    const SceneEntityDefinition& def = exported.getEntities()[0];
    expect_true("export has skeleton", def.has_skeleton);
    expect_true("export two clip bindings", def.animation_player_clips.size() == 2);
    bool saw_edited_walk = false;
    for (const SceneEntityDefinition::AnimationClipBinding& binding :
         def.animation_player_clips) {
      if (binding.name == "walk" && binding.guid == kEditedWalkGuid) {
        saw_edited_walk = true;
      }
    }
    expect_true("export walk guid edited", saw_edited_walk);
  }

  eastl::string json;
  expect_true("serialize edited scene", SceneSerializer::serialize(exported, json));
  Scene loaded;
  expect_true("deserialize edited scene", SceneSerializer::deserialize(json, loaded));
  expect_true("loaded one entity", loaded.getEntities().size() == 1);
  if (loaded.getEntities().size() == 1) {
    bool saw_edited_walk = false;
    for (const SceneEntityDefinition::AnimationClipBinding& binding :
         loaded.getEntities()[0].animation_player_clips) {
      if (binding.name == "walk" && binding.guid == kEditedWalkGuid) {
        saw_edited_walk = true;
      }
    }
    expect_true("deserialize walk guid edited", saw_edited_walk);
  }

  DocumentHistory history;
  history.push(makeSetAnimationPlayerClipBindingsCommand(
      &instance, dog_id, after_bindings, before_bindings,
      SelectionSnapshot{dog_id}, SelectionSnapshot{dog_id}));
  expect_true("undo clip edit", history.undo());
  eastl::string restored_guid;
  expect_true("walk guid restored after undo",
              player->getClipGuid("walk", restored_guid) &&
                  restored_guid == kWalkGuid);
  expect_true("redo clip edit", history.redo());
  expect_true("walk guid re-edited after redo",
              player->getClipGuid("walk", edited_guid) &&
                  edited_guid == kEditedWalkGuid);

  ObjectDB::clear();
}

int main() {
  inspectorClipMapEditPersistsThroughExportAndSerialize();

  using namespace Blunder;
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stderr, "inspector_animation_player_clip_map_test: all passed\n");
  }
  std::fflush(stderr);
  return exit_code;
}
