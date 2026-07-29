#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/script/animation_frame.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;
int g_pose_applied_calls = 0;
int g_tick_event_order = 0;
int g_pose_event_order = 0;
float g_bone_x_at_tick = -1.0f;
float g_bone_x_at_pose_applied = -1.0f;
Blunder::Skeleton* g_test_skeleton = nullptr;

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

void on_tick(void* /*peer*/, float /*dt*/) {
  ++g_tick_calls;
  g_tick_event_order = g_tick_calls + g_pose_applied_calls;
  if (g_test_skeleton != nullptr && g_test_skeleton->getBoneCount() > 0) {
    g_bone_x_at_tick =
        g_test_skeleton->getBonePoseLocal(0).translation.x;
  }
}

void on_pose_applied(Blunder::AnimationPlayer& /*player*/, void* /*userdata*/) {
  ++g_pose_applied_calls;
  g_pose_event_order = g_tick_calls + g_pose_applied_calls;
  if (g_test_skeleton != nullptr && g_test_skeleton->getBoneCount() > 0) {
    g_bone_x_at_pose_applied =
        g_test_skeleton->getBonePoseLocal(0).translation.x;
  }
}

void reset_spies() {
  g_tick_calls = 0;
  g_pose_applied_calls = 0;
  g_tick_event_order = 0;
  g_pose_event_order = 0;
  g_bone_x_at_tick = -1.0f;
  g_bone_x_at_pose_applied = -1.0f;
}

void test_play_frame_order_tick_before_pose_applied() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  reset_spies();

  LifecycleDispatch::setTickHook("Object", on_tick);

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  g_test_skeleton = skeleton;
  skeleton->addBone("Hips", -1);

  const eastl::string guid = "dddddddd-dddd-dddd-dddd-dddddddddddd";
  AnimationClipData clip;
  clip.duration = 2.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {2.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("move", guid);
  player->injectClipData(guid, clip);
  player->addPoseAppliedListener(on_pose_applied, nullptr);
  expect_true("play", player->play("move"));

  reset_spies();
  tickObjectAnimationPlayFrame(object, 1.0f, /*play_paused=*/false);

  expect_true("tick ran", g_tick_calls == 1);
  expect_true("pose applied ran", g_pose_applied_calls == 1);
  expect_true("tick before pose applied", g_tick_event_order < g_pose_event_order);
  expect_true("pose not final at tick", float_near(g_bone_x_at_tick, 0.0f));
  expect_true("pose final at pose applied", float_near(g_bone_x_at_pose_applied, 5.0f));
  expect_true("playback position readable",
              float_near(player->getPlaybackPosition(), 1.0f));
  expect_true("clip length readable", float_near(player->getClipLength(), 2.0f));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_preview_frame_skips_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  reset_spies();

  LifecycleDispatch::setTickHook("Object", on_tick);

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  g_test_skeleton = skeleton;
  skeleton->addBone("Hips", -1);

  const eastl::string guid = "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(4.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("move", guid);
  player->injectClipData(guid, clip);
  player->addPoseAppliedListener(on_pose_applied, nullptr);
  expect_true("play", player->play("move"));

  reset_spies();
  tickObjectAnimationPreviewFrame(object, 0.5f);

  expect_true("preview skips tick", g_tick_calls == 0);
  expect_true("preview still samples", g_pose_applied_calls == 1);
  expect_true("preview pose applied",
              float_near(g_bone_x_at_pose_applied, 2.0f));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_pause_skips_tick_and_advance() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  reset_spies();

  LifecycleDispatch::setTickHook("Object", on_tick);

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  int peer = 0;
  object->addBehaviour("Object");
  object->setBehaviourScriptPeer(object->getBehaviourIdAt(0), &peer);

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  g_test_skeleton = skeleton;
  skeleton->addBone("Hips", -1);

  const eastl::string guid = "ffffffff-ffff-ffff-ffff-ffffffffffff";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(1.0f, 0.0f, 0.0f)}, {1.0f, Vec3(1.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("idle", guid);
  player->injectClipData(guid, clip);
  player->addPoseAppliedListener(on_pose_applied, nullptr);
  expect_true("play", player->play("idle"));

  reset_spies();
  tickObjectAnimationPlayFrame(object, 0.25f, /*play_paused=*/true);

  expect_true("paused skips tick", g_tick_calls == 0);
  expect_true("paused skips advance", g_pose_applied_calls == 0);
  expect_true("position frozen", float_near(player->getPlaybackPosition(), 0.0f));

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

}  // namespace

int main() {
  test_play_frame_order_tick_before_pose_applied();
  test_preview_frame_skips_tick();
  test_pause_skips_tick_and_advance();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("animation_frame_order_test: all passed\n");
  return 0;
}
