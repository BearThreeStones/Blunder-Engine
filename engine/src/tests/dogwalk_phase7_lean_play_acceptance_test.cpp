#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/script/animation_frame.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) < eps;
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b, float eps = 1e-3f) {
  return float_near(a.x, b.x, eps) && float_near(a.y, b.y, eps) &&
         float_near(a.z, b.z, eps);
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

void bindPoseClip(Blunder::AnimationPlayer& player, const char* clip_name,
                  const char* guid, const Blunder::Vec3& translation) {
  Blunder::AnimationClipData clip;
  clip.name = clip_name;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Bone", Blunder::AnimationInterpolation::Constant,
      {{0.0f, translation}, {1.0f, translation}}));
  player.setClipGuid(clip_name, guid);
  player.injectClipData(guid, clip);
}

/// Lean Play: transition condition auto-changes state (no explicit Travel).
void test_lean_play_condition_auto_state_change() {
  using namespace Blunder;
  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  skeleton->addBone("Bone", -1);
  skeleton->setBoneInverseBind(0, Mat4(1.0f));
  skeleton->resetPoseToRest();

  bindPoseClip(*player, "idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
               Vec3(0.0f, 0.0f, 0.0f));
  bindPoseClip(*player, "walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb",
               Vec3(0.0f, 3.0f, 0.0f));

  tree->setStateClip("Idle", "idle");
  tree->setStateClip("Walk", "walk");
  StateMachineTransition edge;
  edge.from_state = "Idle";
  edge.to_state = "Walk";
  edge.source = TransitionConditionSource::TreeParam;
  edge.param_name = "want_walk";
  edge.is_bool_predicate = true;
  edge.bool_operand = true;
  edge.priority = 1;
  expect_true("add transition", tree->addTransition(edge));
  expect_true("activate", tree->setActive(true));
  expect_true("start Idle", tree->start("Idle"));

  // Play-like advance: set condition, then frame tick — no Travel call.
  tree->setTreeParamBool("want_walk", true);
  tickObjectAnimationPlayFrame(object, 0.016f, /*play_paused=*/false);

  expect_true("auto Walk", tree->getCurrentStateName() == "Walk");
  expect_true("walk pose",
              vec3_near(skeleton->getBonePoseLocal(0).translation,
                        Vec3(0.0f, 3.0f, 0.0f)));

  ObjectDB::destroy(id);
  ObjectDB::clear();
}

}  // namespace

int main() {
  test_lean_play_condition_auto_state_change();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase7_lean_play_acceptance_test OK\n");
  return 0;
}
