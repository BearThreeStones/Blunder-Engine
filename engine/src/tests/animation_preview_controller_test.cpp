#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_preview_controller.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;
int g_ready_calls = 0;

constexpr const char* kWalkGuid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";

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

Blunder::Object* makePreviewObject(Blunder::Skeleton** out_skeleton) {
  using namespace Blunder;

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  if (object == nullptr) {
    return nullptr;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  skeleton->addBone("Hips", -1);

  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(2.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("walk", kWalkGuid);
  player->injectClipData(kWalkGuid, clip);

  if (out_skeleton != nullptr) {
    *out_skeleton = skeleton;
  }
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

}  // namespace

int main() {
  test_play_pause_stop_state_machine();
  test_tick_advances_without_behaviour_lifecycle();
  test_loop_toggle();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("animation_preview_controller_test: all passed\n");
  return 0;
}
