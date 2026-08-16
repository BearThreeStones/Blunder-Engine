#include "runtime/core/log/log_system.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/inspector_add_ops.h"
#include "runtime/function/global/global_context.h"
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
  ObjectDB::clear();

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Actor", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    InspectorUniqueAddResult result = applyInspectorUniqueAdd(
        nullptr, scene, id, InspectorUniqueKind::AnimationPlayer);
    expect_true("player add created object", result.created_object);
    expect_true("player add created skeleton", result.created_skeleton);
    expect_true("player add created player", result.created_player);
    Object* object = scene.findBoundObject(id);
    expect_true("has player", object != nullptr && object->hasAnimationPlayer());
    expect_true("has skeleton", object != nullptr && object->hasSkeleton());
    expect_true("clip map empty",
                object != nullptr && object->getAnimationPlayer() != nullptr &&
                    object->getAnimationPlayer()->getClipMapEntryCount() == 0);

    DocumentHistory history;
    history.push(makeAddUniqueAttachmentCommand(
        &scene, nullptr, id, InspectorUniqueKind::AnimationPlayer, result,
        SelectionSnapshot{id}, SelectionSnapshot{id}));
    expect_true("undo add player", history.undo());
    expect_true("undo removed object", scene.findBoundObject(id) == nullptr);
    expect_true("redo add player", history.redo());
    object = scene.findBoundObject(id);
    expect_true("redo player", object != nullptr && object->hasAnimationPlayer());
    expect_true("redo skeleton", object != nullptr && object->hasSkeleton());
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Cam", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    InspectorUniqueAddResult result =
        applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::Camera);
    expect_true("camera created", result.created_camera);
    expect_true("camera present", scene.getCamera(id) != nullptr);
    expect_true("camera created no object", scene.findBoundObject(id) == nullptr);

    DocumentHistory history;
    history.push(makeAddUniqueAttachmentCommand(
        &scene, nullptr, id, InspectorUniqueKind::Camera, result,
        SelectionSnapshot{id}, SelectionSnapshot{id}));
    expect_true("undo add camera", history.undo());
    expect_true("camera removed", scene.getCamera(id) == nullptr);
    expect_true("undo camera still no object", scene.findBoundObject(id) == nullptr);
    expect_true("redo add camera", history.redo());
    expect_true("camera restored", scene.getCamera(id) != nullptr);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Clips", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::AnimationPlayer);
    eastl::vector<AnimationPlayer::ClipBinding> before;
    eastl::vector<AnimationPlayer::ClipBinding> after;
    expect_true("add clip applied", applyInspectorAddClip(scene, id, before, after));
    expect_true("draft row", after.size() == 1 && after[0].name.empty() &&
                                 after[0].guid.empty());
    Object* object = scene.findBoundObject(id);
    expect_true("draft persisted",
                object != nullptr && object->getAnimationPlayer() != nullptr &&
                    object->getAnimationPlayer()->getClipMapEntryCount() == 1);

    DocumentHistory history;
    history.push(makeSetAnimationPlayerClipBindingsCommand(
        &scene, id, before, after, SelectionSnapshot{id}, SelectionSnapshot{id}));
    expect_true("undo add clip", history.undo());
    object = scene.findBoundObject(id);
    expect_true("clip undone",
                object != nullptr && object->getAnimationPlayer() != nullptr &&
                    object->getAnimationPlayer()->getClipMapEntryCount() == 0);
    expect_true("redo add clip", history.redo());
    object = scene.findBoundObject(id);
    expect_true("clip restored",
                object != nullptr && object->getAnimationPlayer() != nullptr &&
                    object->getAnimationPlayer()->getClipMapEntryCount() == 1);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Blocked", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::AnimationPlayer);
    Object* object = scene.findBoundObject(id);
    expect_true("blocked helper", isSkeletonRemoveBlocked(object));
    InspectorUniqueRemoveSnapshot snapshot;
    expect_true("remove skeleton no-op",
                !applyInspectorUniqueRemove(nullptr, scene, id,
                                            InspectorUniqueKind::Skeleton, snapshot));
    expect_true("skeleton remains", object != nullptr && object->hasSkeleton());
    expect_true("player remains", object != nullptr && object->hasAnimationPlayer());
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Tree", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    InspectorUniqueAddResult result = applyInspectorUniqueAdd(
        nullptr, scene, id, InspectorUniqueKind::AnimationTree);
    Object* object = scene.findBoundObject(id);
    expect_true("tree cascade skeleton", result.created_skeleton && result.created_player &&
                                             result.created_tree);
    expect_true("tree present", object != nullptr && object->hasAnimationTree());
    expect_true("tree inactive",
                object != nullptr && object->getAnimationTree() != nullptr &&
                    !object->getAnimationTree()->isActive());
    expect_true("tree no asset guid",
                object != nullptr && object->getAnimationTree() != nullptr &&
                    object->getAnimationTree()->getAssetGuid().empty());

    InspectorUniqueRemoveSnapshot snapshot;
    expect_true("remove player",
                applyInspectorUniqueRemove(nullptr, scene, id,
                                           InspectorUniqueKind::AnimationPlayer,
                                           snapshot));
    object = scene.findBoundObject(id);
    expect_true("remove player keeps skeleton",
                object != nullptr && object->hasSkeleton());
    expect_true("player gone", object != nullptr && !object->hasAnimationPlayer());
  }

  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d inspector_add_menu_commands_test failure(s)\n",
                 g_failures);
    return 1;
  }
  std::fprintf(stderr, "inspector_add_menu_commands_test: all passed\n");
  return 0;
}
