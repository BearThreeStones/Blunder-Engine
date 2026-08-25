#include "runtime/core/log/log_system.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/animation_clip_binding_ops.h"
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
    expect_true("empty draft add rejected",
                !applyInspectorAddClip(scene, id, before, after));
    expect_true(
        "complete binding append",
        applyInspectorAddClipBinding(scene, id, "idle",
                                     "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", before,
                                     after));
    expect_true("one complete row", after.size() == 1 && after[0].name == "idle" &&
                                        !after[0].guid.empty());
    Object* object = scene.findBoundObject(id);
    expect_true("binding persisted",
                object != nullptr && object->getAnimationPlayer() != nullptr &&
                    object->getAnimationPlayer()->getClipMapEntryCount() == 1);

    const eastl::vector<AnimationPlayer::ClipBinding> history_before = before;
    const eastl::vector<AnimationPlayer::ClipBinding> history_after = after;

    eastl::vector<AnimationPlayer::ClipBinding> dup_before;
    eastl::vector<AnimationPlayer::ClipBinding> dup_after;
    expect_true(
        "duplicate name rejected",
        !applyInspectorAddClipBinding(scene, id, "idle",
                                      "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb",
                                      dup_before, dup_after));
    expect_true("still one binding",
                object->getAnimationPlayer()->getClipMapEntryCount() == 1);

    DocumentHistory history;
    history.push(makeSetAnimationPlayerClipBindingsCommand(
        &scene, id, history_before, history_after, SelectionSnapshot{id},
        SelectionSnapshot{id}));
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

    eastl::vector<AnimationPlayer::ClipBinding> retarget_before;
    eastl::vector<AnimationPlayer::ClipBinding> retarget_after;
    expect_true(
        "drop retarget row 0",
        applyInspectorAnimationClipDrop(
            scene, id, 0, "ignored-stem",
            "cccccccc-cccc-4ccc-8ccc-cccccccccccc", retarget_before,
            retarget_after));
    expect_true("retarget keeps name", retarget_after.size() == 1 &&
                                           retarget_after[0].name == "idle" &&
                                           retarget_after[0].guid ==
                                               "cccccccc-cccc-4ccc-8ccc-cccccccccccc");

    eastl::vector<AnimationPlayer::ClipBinding> append_before;
    eastl::vector<AnimationPlayer::ClipBinding> append_after;
    expect_true(
        "drop append",
        applyInspectorAnimationClipDrop(
            scene, id, k_animation_clip_drop_append, "walk",
            "dddddddd-dddd-4ddd-8ddd-dddddddddddd", append_before, append_after));
    expect_true("two bindings after append drop", append_after.size() == 2 &&
                                                      append_after[1].name == "walk");
    expect_true(
        "drop miss no-op",
        !applyInspectorAnimationClipDrop(
            scene, id, k_animation_clip_drop_miss, "run",
            "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee", append_before, append_after));
  }

  {
    expect_true(
        "hit row 0",
        hitTestAnimationClipDropTarget(10.0f, 10.0f, 0.0f, 0.0f, 100.0f, 56.0f, 2,
                                       120.0f, 24.0f) == 0);
    expect_true(
        "hit row 1",
        hitTestAnimationClipDropTarget(10.0f, 60.0f, 0.0f, 0.0f, 100.0f, 56.0f, 2,
                                       120.0f, 24.0f) == 1);
    expect_true(
        "hit add clip",
        hitTestAnimationClipDropTarget(10.0f, 125.0f, 0.0f, 0.0f, 100.0f, 56.0f, 2,
                                       120.0f, 24.0f) ==
            k_animation_clip_drop_append);
    expect_true(
        "hit miss outside",
        hitTestAnimationClipDropTarget(200.0f, 10.0f, 0.0f, 0.0f, 100.0f, 56.0f, 2,
                                       120.0f, 24.0f) == k_animation_clip_drop_miss);
    expect_true(
        "empty list append chrome",
        hitTestAnimationClipDropTarget(10.0f, 10.0f, 0.0f, 0.0f, 100.0f, 56.0f, 0,
                                       0.0f, 24.0f) ==
            k_animation_clip_drop_append);
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

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Lamp", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    InspectorUniqueAddResult result =
        applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::Light);
    expect_true("light created", result.created_light);
    expect_true("light present", scene.getLight(id) != nullptr);
    expect_true("light default directional",
                scene.getLight(id) != nullptr &&
                    scene.getLight(id)->type == LightType::directional);
    expect_true("light created no object", scene.findBoundObject(id) == nullptr);

    DocumentHistory history;
    history.push(makeAddUniqueAttachmentCommand(
        &scene, nullptr, id, InspectorUniqueKind::Light, result,
        SelectionSnapshot{id}, SelectionSnapshot{id}));
    expect_true("undo add light", history.undo());
    expect_true("light removed", scene.getLight(id) == nullptr);
    expect_true("undo light still no object", scene.findBoundObject(id) == nullptr);
    expect_true("redo add light", history.redo());
    expect_true("light restored", scene.getLight(id) != nullptr);

    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::Light);
    expect_true("unique already-present",
                applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::Light)
                    .already_present);

    const LightComponent before = *scene.getLight(id);
    LightComponent after = before;
    after.type = LightType::point;
    scene.setLight(id, after);
    history.push(makeSetLightComponentCommand(
        &scene, id, before, after, SelectionSnapshot{id}, SelectionSnapshot{id}));
    expect_true("undo type change", history.undo() && scene.getLight(id) != nullptr &&
                                        scene.getLight(id)->type == LightType::directional);
    expect_true("redo type change", history.redo() && scene.getLight(id) != nullptr &&
                                        scene.getLight(id)->type == LightType::point);

    applyInspectorUniqueAdd(nullptr, scene, id, InspectorUniqueKind::Camera);
    expect_true("camera+light coexist", scene.getCamera(id) != nullptr &&
                                            scene.getLight(id) != nullptr);
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
