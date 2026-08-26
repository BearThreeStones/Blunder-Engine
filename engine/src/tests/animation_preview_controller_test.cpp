#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_preview_controller.h"
#include "runtime/core/log/log_system.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/entity_id.h"

#include "EASTL/shared_ptr.h"
#include "EASTL/unique_ptr.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;
int g_ready_calls = 0;

constexpr const char* kWalkGuid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
constexpr const char* kIdleGuid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
constexpr const char* kTurnGuid = "cccccccc-cccc-cccc-cccc-cccccccccccc";

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

Blunder::AnimationTrack makeTranslationTrack(
    const char* bone, Blunder::AnimationInterpolation interpolation,
    std::initializer_list<std::pair<float, Blunder::Vec3>> keys) {
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = Blunder::AnimationChannel::Translation;
  track.interpolation = interpolation;
  for (const auto& key : keys) {
    Blunder::AnimationKeyframe frame;
    frame.time = key.first;
    frame.value = {key.second.x, key.second.y, key.second.z};
    track.keys.push_back(frame);
  }
  return track;
}

void on_tick(void* /*peer*/, float /*dt*/) { ++g_tick_calls; }

void on_ready(void* /*peer*/) { ++g_ready_calls; }

Blunder::Object* makePreviewObject(Blunder::Skeleton** out_skeleton,
                                     bool two_clips = false) {
  using namespace Blunder;

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  if (object == nullptr) {
    return nullptr;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  skeleton->addBone("Hips", -1);

  AnimationClipData walk_clip;
  walk_clip.duration = 1.0f;
  walk_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(2.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("walk", kWalkGuid);
  player->injectClipData(kWalkGuid, walk_clip);

  if (two_clips) {
    AnimationClipData idle_clip;
    idle_clip.duration = 1.0f;
    idle_clip.tracks.push_back(makeTranslationTrack(
        "Hips", AnimationInterpolation::Linear,
        {{0.0f, Vec3(4.0f, 0.0f, 0.0f)}}));
    AnimationClipData walk_at_zero;
    walk_at_zero.duration = 1.0f;
    walk_at_zero.tracks.push_back(makeTranslationTrack(
        "Hips", AnimationInterpolation::Linear,
        {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
    player->setClipGuid("idle", kIdleGuid);
    player->injectClipData(kIdleGuid, idle_clip);
    player->injectClipData(kWalkGuid, walk_at_zero);
  }

  if (out_skeleton != nullptr) {
    *out_skeleton = skeleton;
  }
  return object;
}

Blunder::Object* makeTreePreviewObject(Blunder::Skeleton** out_skeleton) {
  using namespace Blunder;

  Object* object = makePreviewObject(out_skeleton, true);
  if (object == nullptr) {
    return nullptr;
  }

  AnimationTree* tree = object->ensureAnimationTree();
  AnimationPlayer* player = object->getAnimationPlayer();
  if (tree == nullptr || player == nullptr) {
    return nullptr;
  }

  tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
  tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
  tree->setStateBlendSpace("Locomotion", "Locomotion");
  return object;
}

void test_play_pause_stop_state_machine() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makePreviewObject(nullptr);
  expect_true("object created", object != nullptr);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  expect_true("play enabled", controller.playEnabled());
  expect_true("play starts", controller.play());
  expect_true("playing", controller.state() == AnimationPreviewState::Playing);
  expect_true("pause enabled", controller.pauseEnabled());
  expect_true("pause", controller.pause());
  expect_true("paused", controller.isPaused());
  expect_true("resume", controller.resume());
  expect_true("stop", controller.stopEnabled());
  controller.stop();
  expect_true("stopped", controller.state() == AnimationPreviewState::Stopped);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_tick_advances_without_behaviour_lifecycle() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  g_ready_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::setReadyHook("Object", on_ready);

  Skeleton* skeleton = nullptr;
  Object* object = makePreviewObject(&skeleton);
  expect_true("object created", object != nullptr);
  expect_true("skeleton", skeleton != nullptr);

  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  expect_true("play", controller.play());
  expect_true("play skips lifecycle", g_tick_calls == 0 && g_ready_calls == 0);

  controller.tick(0.5f);
  expect_true("preview skips behaviour tick", g_tick_calls == 0);
  expect_true("preview skips behaviour ready", g_ready_calls == 0);
  expect_true("position advanced", float_near(controller.playbackPosition(), 0.5f));
  expect_true("bone sampled",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 1.0f));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_time_scale_scrub_affects_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makePreviewObject(nullptr);
  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  controller.setTimeScale(2.0f);
  expect_true("play", controller.play());
  controller.tick(0.25f);
  expect_true("timeScale doubles advance",
              float_near(controller.playbackPosition(), 0.5f));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_blend_weight_scrub_updates_player() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Skeleton* skeleton = nullptr;
  Object* object = makePreviewObject(&skeleton, true);
  AnimationPlayer* player = object->getAnimationPlayer();
  expect_true("slots assigned", player->setSlot(0, "idle") && player->setSlot(1, "walk"));

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  controller.setBlendWeight(0.5f);
  expect_true("blend on player", float_near(player->getBlendWeight(), 0.5f));
  expect_true("blend sampled midpoint",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 2.0f));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_play_uses_fade_duration() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makePreviewObject(nullptr, true);
  AnimationPlayer* player = object->getAnimationPlayer();
  expect_true("slot0 idle", player->setSlot(0, "idle"));

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  controller.setFadeSeconds(0.5f);
  expect_true("play with fade", controller.play("walk"));
  expect_true("crossfading", player->isCrossfading());

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_set_slot_updates_player() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makePreviewObject(nullptr, true);
  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  expect_true("set slot0", controller.setSlot(0, "idle"));
  expect_true("slot0 name", controller.slotClipName(0) == "idle");

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_loop_toggle() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makePreviewObject(nullptr);
  AnimationPreviewController controller;
  controller.bindObject(object, "walk");

  expect_true("loop off initially", !controller.isLooping());
  controller.toggleLoop();
  expect_true("loop on", controller.isLooping());
  controller.setLoop(false);
  expect_true("loop off", !controller.isLooping());

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

void test_tree_window_bind_enable_disable() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* tree_object = makeTreePreviewObject(nullptr);
  Object* player_object = makePreviewObject(nullptr);
  AnimationPreviewController controller;
  controller.bindObject(tree_object, "walk");
  expect_true("tree window bound", controller.windowBound());
  expect_true("tree play enabled", controller.playEnabled());

  controller.bindObject(player_object, "walk");
  expect_true("player-only not window bound", !controller.windowBound());
  controller.clearTarget();

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_tree_stop_and_rebind() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* first = makeTreePreviewObject(nullptr);
  Object* second = makeTreePreviewObject(nullptr);
  AnimationTree* first_tree = first->getAnimationTree();
  expect_true("first tree", first_tree != nullptr);
  first_tree->start("Locomotion");
  first_tree->setActive(true);

  AnimationPreviewController controller;
  controller.bindObject(first, "walk");
  expect_true("play first", controller.play());
  expect_true("fire", controller.requestOneShot("walk"));
  controller.enterCine();
  expect_true("cine on", controller.isInCine());
  expect_true("oneshot on", first_tree->isOneShotActive());

  controller.stop();
  expect_true("stopped", controller.state() == AnimationPreviewState::Stopped);
  expect_true("seek 0", float_near(controller.playbackPosition(), 0.0f));
  expect_true("fire cleared", !first_tree->isOneShotActive());
  expect_true("cine ended", !controller.isInCine());
  expect_true("tree stays active", first_tree->isActive());

  controller.bindObject(first, "walk");
  controller.play();
  controller.requestOneShot("walk");
  controller.enterCine();
  controller.bindSelection(nullptr, k_invalid_entity_id, 0);
  expect_true("unbind stopped",
              controller.state() == AnimationPreviewState::Stopped);
  expect_true("unbind cine off", !controller.isInCine());
  expect_true("unbind fire cleared", !first_tree->isOneShotActive());

  controller.bindObject(second, "walk");
  expect_true("second bound", controller.windowBound());
  controller.clearTarget();

  ObjectDB::clear();
  LifecycleDispatch::clear();

  ensureLogger();
  Scene scene;
  scene.setGuid("eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee");
  SceneEntityDefinition dog_a;
  dog_a.name = "DogA";
  dog_a.has_skeleton = true;
  dog_a.has_animation_tree = true;
  dog_a.animation_player_clips.push_back({"walk", eastl::string(kWalkGuid)});
  SceneEntityDefinition dog_b = dog_a;
  dog_b.name = "DogB";
  scene.getEntities().push_back(eastl::move(dog_a));
  scene.getEntities().push_back(eastl::move(dog_b));

  SceneInstance instance;
  instance.instantiate(scene);
  const EntityId id_a = instance.findEntityByName("DogA");
  const EntityId id_b = instance.findEntityByName("DogB");
  expect_true("dog A", isValid(id_a));
  expect_true("dog B", isValid(id_b));
  Object* object_a = instance.findBoundObject(id_a);
  expect_true("object A", object_a != nullptr && object_a->hasAnimationTree());
  AnimationTree* tree_a = object_a->getAnimationTree();

  controller.bindSelection(&instance, id_a, 1);
  expect_true("select A", controller.windowBound());
  expect_true("play A", controller.play());
  expect_true("fire A", controller.requestOneShot("walk"));
  controller.enterCine();
  expect_true("cine A", controller.isInCine());
  expect_true("oneshot A", tree_a != nullptr && tree_a->isOneShotActive());

  controller.bindSelection(&instance, id_b, 1);
  expect_true("select B", controller.windowBound());
  expect_true("rebind halted A oneshot",
              tree_a != nullptr && !tree_a->isOneShotActive());
  expect_true("rebind ended cine", !controller.isInCine());
  expect_true("rebind stopped",
              controller.state() == AnimationPreviewState::Stopped);
  controller.clearTarget();
}

void test_session_loop_off_pauses_last_frame() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makeTreePreviewObject(nullptr);
  AnimationTree* tree = object->getAnimationTree();
  tree->start("Locomotion");
  tree->setActive(true);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  controller.setLoop(false);
  expect_true("play", controller.play());
  controller.tick(2.0f);
  expect_true("paused at end", controller.isPaused());
  expect_true("last frame",
              float_near(controller.playbackPosition(), 1.0f, 1.0e-3f));

  expect_true("play from last frame", controller.play());
  expect_true("restarted at 0",
              float_near(controller.playbackPosition(), 0.0f, 1.0e-3f));
  expect_true("playing again", controller.isPlaying());

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_fire_hard_cut_and_enter_cine_does_not_fire() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makeTreePreviewObject(nullptr);
  AnimationTree* tree = object->getAnimationTree();
  tree->start("Locomotion");
  tree->setActive(true);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  controller.setFireTarget("walk");
  expect_true("fire walk", controller.fire());
  expect_true("oneshot walk", tree->isOneShotActive());
  expect_true("oneshot name walk", tree->getOneShotClipName() == "walk");

  controller.setFireTarget("idle");
  expect_true("hard-cut idle", controller.fire());
  expect_true("oneshot idle", tree->getOneShotClipName() == "idle");

  tree->clearOneShot();
  controller.enterCine();
  expect_true("cine on", controller.isInCine());
  expect_true("enter did not fire", !tree->isOneShotActive());

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_clip_play_stop_rebind_and_end_cine() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makeTreePreviewObject(nullptr);
  AnimationTree* tree = object->getAnimationTree();
  tree->start("Locomotion");
  tree->setActive(true);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  expect_true("clip play", tree->clipPlay("walk"));
  expect_true("override on", tree->isClipPlayOverride());
  expect_true("ruler walk", controller.rulerClipName() == "walk");

  controller.enterCine();
  controller.endCine();
  expect_true("end cine keeps override", tree->isClipPlayOverride());
  expect_true("ruler still walk", controller.rulerClipName() == "walk");

  controller.stop();
  expect_true("stop clears override", !tree->isClipPlayOverride());

  ObjectDB::clear();
  LifecycleDispatch::clear();

  ensureLogger();
  Scene scene;
  scene.setGuid("ffffffff-ffff-4fff-8fff-ffffffffffff");
  SceneEntityDefinition dog_a;
  dog_a.name = "DogA";
  dog_a.has_skeleton = true;
  dog_a.has_animation_tree = true;
  dog_a.animation_player_clips.push_back({"walk", eastl::string(kWalkGuid)});
  SceneEntityDefinition dog_b = dog_a;
  dog_b.name = "DogB";
  scene.getEntities().push_back(eastl::move(dog_a));
  scene.getEntities().push_back(eastl::move(dog_b));

  SceneInstance instance;
  instance.instantiate(scene);
  const EntityId id_a = instance.findEntityByName("DogA");
  const EntityId id_b = instance.findEntityByName("DogB");
  expect_true("dog A", isValid(id_a));
  expect_true("dog B", isValid(id_b));
  Object* object_a = instance.findBoundObject(id_a);
  expect_true("object A", object_a != nullptr && object_a->hasAnimationTree());
  AnimationTree* tree_a = object_a->getAnimationTree();

  controller.bindSelection(&instance, id_a, 1);
  expect_true("play A", controller.play());
  expect_true("clip play A", tree_a != nullptr && tree_a->clipPlay("walk"));
  expect_true("clip play A set",
              tree_a != nullptr && tree_a->isClipPlayOverride());

  controller.bindSelection(&instance, id_b, 1);
  expect_true("rebind cleared clip play",
              tree_a != nullptr && !tree_a->isClipPlayOverride());
  controller.clearTarget();
}

void test_timescale_command_dirties_play_does_not() {
  using namespace Blunder;

  ensureLogger();
  ObjectDB::clear();
  LifecycleDispatch::clear();

  Scene scene;
  scene.setGuid("dddddddd-dddd-4ddd-8ddd-dddddddddddd");
  SceneEntityDefinition entity;
  entity.name = "Dog";
  entity.has_skeleton = true;
  entity.has_animation_tree = true;
  entity.animation_player_clips.push_back({"walk", eastl::string(kWalkGuid)});
  SceneEntityDefinition camera;
  camera.name = "Camera";
  scene.getEntities().push_back(eastl::move(entity));
  scene.getEntities().push_back(eastl::move(camera));

  SceneInstance instance;
  instance.instantiate(scene);
  const EntityId dog_id = instance.findEntityByName("Dog");
  const EntityId camera_id = instance.findEntityByName("Camera");
  expect_true("dog entity", isValid(dog_id));
  expect_true("camera entity", isValid(camera_id));
  Object* object = instance.findBoundObject(dog_id);
  expect_true("bound object", object != nullptr);
  expect_true("has tree", object != nullptr && object->hasAnimationTree());

  AnimationPreviewController controller;
  controller.bindSelection(&instance, dog_id, 1);
  expect_true("window bound", controller.windowBound());
  controller.bindSelection(&instance, dog_id, 2);
  expect_true("multi unbound", !controller.windowBound());
  controller.bindSelection(&instance, camera_id, 1);
  expect_true("camera unbound", !controller.windowBound());
  controller.bindSelection(&instance, dog_id, 1);
  expect_true("rebound dog", controller.windowBound());

  DocumentHistory history;
  history.markSaveBaseline();
  expect_true("play starts", controller.play());
  expect_true("play does not dirty", !history.isDirtyRelativeToSave());

  const float before = controller.timeScale();
  controller.setTimeScale(0.5f);
  expect_true("live scale not a command", !history.isDirtyRelativeToSave());
  SelectionSnapshot snap;
  snap.primary = dog_id;
  history.push(makeSetAnimationPlayerTimeScaleCommand(&instance, dog_id, before,
                                                      0.5f, snap, snap));
  expect_true("timescale command dirties", history.isDirtyRelativeToSave());
  expect_true("can undo timescale", history.canUndo());
  controller.clearTarget();
}

void test_preview_scrub_paths_skip_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  g_ready_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::setReadyHook("Object", on_ready);

  Object* object = makePreviewObject(nullptr, true);
  expect_true("object created", object != nullptr);

  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");

  controller.setTimeScale(2.0f);
  expect_true("timeScale scrub skips tick",
              g_tick_calls == 0 && g_ready_calls == 0);

  expect_true("play", controller.play());
  expect_true("play skips tick", g_tick_calls == 0 && g_ready_calls == 0);

  controller.tick(0.25f);
  expect_true("preview tick skips behaviour", g_tick_calls == 0);
  expect_true("preview tick skips ready", g_ready_calls == 0);
  expect_true("timeScale advances preview",
              float_near(controller.playbackPosition(), 0.5f));

  controller.setBlendWeight(0.75f);
  expect_true("blendWeight scrub skips tick",
              g_tick_calls == 0 && g_ready_calls == 0);

  expect_true("set slot0", controller.setSlot(0, "idle"));
  expect_true("setSlot skips tick", g_tick_calls == 0 && g_ready_calls == 0);

  controller.setFadeSeconds(0.5f);
  expect_true("play fade", controller.play("walk"));
  expect_true("play fade skips tick", g_tick_calls == 0 && g_ready_calls == 0);

  controller.tick(0.1f);
  expect_true("tick during crossfade skips behaviour", g_tick_calls == 0);
  expect_true("tick during crossfade skips ready", g_ready_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_tree_activate_and_scrub_without_behaviour_lifecycle() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  g_ready_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::setReadyHook("Object", on_ready);

  Skeleton* skeleton = nullptr;
  Object* object = makeTreePreviewObject(&skeleton);
  expect_true("object created", object != nullptr);

  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  expect_true("has tree", controller.hasTree());
  expect_true("activate tree", controller.setTreeActive(true));
  expect_true("tree active", controller.isTreeActive());
  expect_true("activate skips tick", g_tick_calls == 0 && g_ready_calls == 0);

  expect_true("travel", controller.travel("Locomotion"));
  expect_true("start", controller.start("Locomotion"));
  expect_true("travel skips tick", g_tick_calls == 0 && g_ready_calls == 0);

  controller.setBlendSpaceScalar("Locomotion", 0.5f);
  expect_true("blend scalar applied",
              float_near(controller.blendSpaceScalar("Locomotion"), 0.5f));
  expect_true("blend scalar skips tick", g_tick_calls == 0);
  expect_true("blend sampled midpoint",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 2.0f));

  controller.setTimeScale(2.0f);
  expect_true("play", controller.play());
  controller.tick(0.25f);
  expect_true("tree preview tick skips behaviour", g_tick_calls == 0);
  expect_true("tree preview tick skips ready", g_ready_calls == 0);
  expect_true("timeScale advances tree preview",
              float_near(controller.playbackPosition(), 0.5f));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_tree_oneshot_and_add2_scrub_without_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  g_ready_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::setReadyHook("Object", on_ready);

  Skeleton* skeleton = nullptr;
  Object* object = makeTreePreviewObject(&skeleton);
  AnimationTree* tree = object->getAnimationTree();
  expect_true("tree", tree != nullptr);

  AnimationClipData turn_clip;
  turn_clip.duration = 1.0f;
  turn_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(6.0f, 0.0f, 0.0f)}}));
  object->getAnimationPlayer()->setClipGuid("turn", kTurnGuid);
  object->getAnimationPlayer()->injectClipData(kTurnGuid, turn_clip);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");
  expect_true("activate", controller.setTreeActive(true));
  expect_true("travel", controller.travel("Locomotion"));

  expect_true("set add2 clip", controller.setAdd2ClipName("turn"));
  controller.setAdd2Weight(0.5f);
  expect_true("add2 weight", float_near(controller.add2Weight(), 0.5f));
  expect_true("add2 scrub skips tick", g_tick_calls == 0 && g_ready_calls == 0);

  expect_true("request oneshot", controller.requestOneShot("walk"));
  expect_true("oneshot active", tree->isOneShotActive());
  expect_true("oneshot scrub skips tick", g_tick_calls == 0 && g_ready_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_edit_tree_scrub_requires_no_graph_editor_or_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  g_ready_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);
  LifecycleDispatch::setReadyHook("Object", on_ready);

  Object* object = makeTreePreviewObject(nullptr);
  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");

  expect_true("scene-embedded tree only (no graph editor API)",
              controller.hasTree());
  expect_true("setTreeActive", controller.setTreeActive(true));
  controller.setBlendSpaceScalar("Locomotion", 0.25f);
  controller.setTimeScale(1.5f);
  expect_true("play", controller.play());
  controller.tick(0.1f);

  expect_true("no behaviour tick during tree edit scrub", g_tick_calls == 0);
  expect_true("no behaviour ready during tree edit scrub", g_ready_calls == 0);
  expect_true("tree still active after scrub", controller.isTreeActive());

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_edit_modifier_enable_order_without_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* object = makeTreePreviewObject(&skeleton);
  expect_true("object", object != nullptr);

  auto first = eastl::make_unique<SkeletonModifier>();
  first->setApplyFn(
      [](Skeleton& skel, void*) {
        BoneTransform pose = skel.getBonePoseLocal(0);
        pose.translation.x += 1.0f;
        skel.setBonePoseLocal(0, pose);
      },
      nullptr);
  object->addSkeletonModifier(eastl::move(first));

  auto second = eastl::make_unique<SkeletonModifier>();
  second->setApplyFn(
      [](Skeleton& skel, void*) {
        BoneTransform pose = skel.getBonePoseLocal(0);
        pose.translation.x += 10.0f;
        skel.setBonePoseLocal(0, pose);
      },
      nullptr);
  object->addSkeletonModifier(eastl::move(second));

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  expect_true("travel", controller.travel("Locomotion"));
  expect_true("activate", controller.setTreeActive(true));
  controller.setBlendSpaceScalar("Locomotion", 0.0f);

  expect_true("two modifiers", controller.skeletonModifierCount() == 2);
  const float with_both = skeleton->getBonePoseLocal(0).translation.x;
  expect_true("order +1 then +10", float_near(with_both, 15.0f));
  if (!float_near(with_both, 15.0f)) {
    std::fprintf(stderr, "  got with_both=%f\n", with_both);
  }

  expect_true("disable second",
              controller.setSkeletonModifierEnabled(1, false));
  const float with_first = skeleton->getBonePoseLocal(0).translation.x;
  expect_true("only first applies", float_near(with_first, 5.0f));
  if (!float_near(with_first, 5.0f)) {
    std::fprintf(stderr, "  got with_first=%f\n", with_first);
  }

  expect_true("re-enable second",
              controller.setSkeletonModifierEnabled(1, true));
  expect_true("move second before first",
              controller.moveSkeletonModifier(1, 0));
  const float reordered = skeleton->getBonePoseLocal(0).translation.x;
  expect_true("order +10 then +1", float_near(reordered, 15.0f));
  if (!float_near(reordered, 15.0f)) {
    std::fprintf(stderr, "  got reordered=%f\n", reordered);
  }
  expect_true("no behaviour tick", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_edit_blend_space2d_scrub_without_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* object = makeTreePreviewObject(&skeleton);
  AnimationTree* tree = object->getAnimationTree();
  tree->clearAuthoredTopology();
  tree->addBlendSpace2DPoint("Locomotion2D", "idle", 0.0f, 0.0f);
  tree->addBlendSpace2DPoint("Locomotion2D", "walk", 1.0f, 0.0f);
  // Need a third point for triangle — reuse walk at different y via another clip.
  // walk is enough with 2-point line blend for scrub API.
  tree->setBaseBlendSpace2DNode("Locomotion2D");

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  expect_true("activate", controller.setTreeActive(true));
  controller.setBlendSpace2DParam("Locomotion2D", 0.5f, 0.0f);

  float x = 0.0f;
  float y = 0.0f;
  expect_true("read param", controller.blendSpace2DParam("Locomotion2D", x, y));
  expect_true("param x", float_near(x, 0.5f));
  expect_true("param y", float_near(y, 0.0f));
  expect_true("2d midpoint pose",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 2.0f));
  expect_true("no behaviour tick", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_edit_tree_asset_and_overrides_without_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* object = makeTreePreviewObject(&skeleton);

  AnimationTreeTopologyData topology;
  topology.base_blend_space_node = "Locomotion";
  AnimationTreeTopologyData::BlendSpace1DDef space;
  space.node_name = "Locomotion";
  space.scalar = 0.0f;
  space.points.push_back({"idle", 0.0f});
  space.points.push_back({"walk", 1.0f});
  topology.blend_spaces_1d.push_back(eastl::move(space));

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  controller.setAssetGuid("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  expect_true("apply topology", controller.applyTreeTopology(topology));
  expect_true("guid",
              controller.assetGuid() == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");

  AnimationTreeInstanceOverrides overrides;
  overrides.blend_space_scalars.push_back({"Locomotion", 1.0f});
  overrides.has_active = true;
  overrides.active = true;
  controller.applyTreeOverrides(overrides);

  expect_true("override walk pose",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 0.0f));
  expect_true("no behaviour tick", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_edit_method_scrub_markers_without_behaviour_handling() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* object = makeTreePreviewObject(&skeleton);
  AnimationPlayer* player = object->getAnimationPlayer();

  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  AnimationMethodKey key;
  key.name = "FootStep";
  key.time = 0.25f;
  clip.method_keys.push_back(key);
  player->injectClipData(kIdleGuid, clip);

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  controller.setTreeActive(true);
  controller.setBlendSpaceScalar("Locomotion", 0.0f);
  controller.play();
  controller.tick(0.3f);

  // Method keys remain on clip for Edit markers/logs; Behaviour handling is Play-only.
  AnimationClipData loaded;
  expect_true("resolve clip", player->resolveClipForName("idle", loaded));
  expect_true("method marker present",
              loaded.method_keys.size() == 1 &&
                  loaded.method_keys[0].name == "FootStep");
  expect_true("scrub without behaviour handling", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_skeleton_modifier_setters_reject_wrong_type() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();

  Object* object = makePreviewObject(nullptr);
  expect_true("object created", object != nullptr);
  object->addSkeletonPaperMouthModifier();
  object->addSkeletonLookAtModifier();
  object->addSkeletonAttachModifier();

  AnimationPreviewController controller;
  controller.bindObject(object, "walk");

  expect_true("lookAt target rejects PaperMouth",
              !controller.setSkeletonLookAtTarget(0, Vec3(1.0f, 2.0f, 3.0f)));
  expect_true("lookAt bone rejects PaperMouth",
              !controller.setSkeletonLookAtBoneName(0, "Hips"));
  expect_true("paper mouth open rejects LookAt",
              !controller.setSkeletonPaperMouthOpenAmount(1, 0.5f));
  expect_true("paper mouth bone rejects LookAt",
              !controller.setSkeletonPaperMouthBoneName(1, "Hips"));
  expect_true("attach bone rejects LookAt",
              !controller.setSkeletonAttachBoneName(1, "Hips"));
  expect_true("attach child rejects PaperMouth",
              !controller.setSkeletonAttachChildObjectId(0, ObjectId{1}));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

}  // namespace

int main() {
  test_play_pause_stop_state_machine();
  test_tick_advances_without_behaviour_lifecycle();
  test_preview_scrub_paths_skip_behaviour_tick();
  test_tree_activate_and_scrub_without_behaviour_lifecycle();
  test_tree_oneshot_and_add2_scrub_without_behaviour_tick();
  test_edit_tree_scrub_requires_no_graph_editor_or_behaviour_tick();
  test_edit_modifier_enable_order_without_behaviour_tick();
  test_edit_blend_space2d_scrub_without_behaviour_tick();
  test_edit_tree_asset_and_overrides_without_behaviour_tick();
  test_edit_method_scrub_markers_without_behaviour_handling();
  test_skeleton_modifier_setters_reject_wrong_type();
  test_time_scale_scrub_affects_tick();
  test_blend_weight_scrub_updates_player();
  test_play_uses_fade_duration();
  test_set_slot_updates_player();
  test_loop_toggle();
  test_tree_window_bind_enable_disable();
  test_tree_stop_and_rebind();
  test_session_loop_off_pauses_last_frame();
  test_fire_hard_cut_and_enter_cine_does_not_fire();
  test_clip_play_stop_rebind_and_end_cine();
  test_timescale_command_dirties_play_does_not();

  Blunder::ObjectDB::clear();
  Blunder::LifecycleDispatch::clear();
  Blunder::g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("animation_preview_controller_test: all passed\n");
  std::fflush(stdout);
  return 0;
}
