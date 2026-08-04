#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/function/script/animation_frame.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_event_counter = 0;
int g_modifier_event_order = 0;
int g_pose_applied_event_order = 0;
float g_bone_x_at_modifier = -1.0f;
float g_bone_x_at_pose_applied = -1.0f;

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

void reset_spies() {
  g_event_counter = 0;
  g_modifier_event_order = 0;
  g_pose_applied_event_order = 0;
  g_bone_x_at_modifier = -1.0f;
  g_bone_x_at_pose_applied = -1.0f;
}

void offset_bone_x_modifier(Blunder::Skeleton& skeleton, void* /*userdata*/) {
  g_modifier_event_order = ++g_event_counter;
  if (skeleton.getBoneCount() > 0) {
    g_bone_x_at_modifier = skeleton.getBonePoseLocal(0).translation.x;
    Blunder::BoneTransform pose = skeleton.getBonePoseLocal(0);
    pose.translation.x += 100.0f;
    skeleton.setBonePoseLocal(0, pose);
  }
}

void on_pose_applied(Blunder::AnimationPlayer& /*player*/, void* /*userdata*/) {
  g_pose_applied_event_order = ++g_event_counter;
}

void test_player_modifier_runs_after_sample_before_pose_applied() {
  using namespace Blunder;

  ObjectDB::clear();
  reset_spies();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  skeleton->addBone("Hips", -1);

  SkeletonModifier* modifier = object->addSkeletonModifier();
  expect_true("modifier created", modifier != nullptr);
  if (modifier == nullptr) {
    return;
  }
  modifier->setApplyFn(offset_bone_x_modifier, nullptr);

  const eastl::string guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  AnimationClipData clip;
  clip.duration = 2.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {2.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("move", guid);
  player->injectClipData(guid, clip);
  player->addPoseAppliedListener(on_pose_applied, skeleton);
  expect_true("play", player->play("move"));

  reset_spies();
  tickObjectAnimationPlayFrame(object, 1.0f, /*play_paused=*/false);

  expect_true("modifier ran", g_modifier_event_order > 0);
  expect_true("pose applied ran", g_pose_applied_event_order > 0);
  expect_true("modifier before pose applied",
              g_modifier_event_order < g_pose_applied_event_order);
  expect_true("modifier sees sampled pose", float_near(g_bone_x_at_modifier, 5.0f));
  expect_true("pose applied sees post-modifier pose",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 105.0f));

  ObjectDB::clear();
}

void test_tree_modifier_runs_after_sample_before_pose_applied() {
  using namespace Blunder;

  ObjectDB::clear();
  reset_spies();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  skeleton->addBone("Hips", -1);

  SkeletonModifier* modifier = object->addSkeletonModifier();
  expect_true("modifier created", modifier != nullptr);
  if (modifier == nullptr) {
    return;
  }
  modifier->setApplyFn(offset_bone_x_modifier, nullptr);

  const eastl::string guid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(8.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("walk", guid);
  player->injectClipData(guid, clip);
  player->addPoseAppliedListener(on_pose_applied, skeleton);
  expect_true("tree sample clip", tree->setSampleClipName("walk"));
  expect_true("tree active", tree->setActive(true));
  expect_true("player play for clock", player->play("walk"));

  reset_spies();
  tickObjectAnimationPlayFrame(object, 0.5f, /*play_paused=*/false);

  expect_true("modifier ran", g_modifier_event_order > 0);
  expect_true("pose applied ran", g_pose_applied_event_order > 0);
  expect_true("modifier before pose applied",
              g_modifier_event_order < g_pose_applied_event_order);
  expect_true("modifier sees sampled pose", float_near(g_bone_x_at_modifier, 4.0f));
  expect_true("pose applied sees post-modifier pose",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 104.0f));

  ObjectDB::clear();
}

void test_disabled_modifier_skipped() {
  using namespace Blunder;

  ObjectDB::clear();
  reset_spies();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  skeleton->addBone("Hips", -1);

  SkeletonModifier* modifier = object->addSkeletonModifier();
  expect_true("modifier created", modifier != nullptr);
  if (modifier == nullptr) {
    return;
  }
  modifier->setApplyFn(offset_bone_x_modifier, nullptr);
  modifier->setEnabled(false);

  const eastl::string guid = "cccccccc-cccc-cccc-cccc-cccccccccccc";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(6.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("move", guid);
  player->injectClipData(guid, clip);
  expect_true("play", player->play("move"));

  reset_spies();
  tickObjectAnimationPlayFrame(object, 0.5f, /*play_paused=*/false);

  expect_true("disabled modifier skipped", g_modifier_event_order == 0);
  expect_true("sampled pose unchanged", float_near(skeleton->getBonePoseLocal(0).translation.x, 3.0f));

  ObjectDB::clear();
}

}  // namespace

int main() {
  test_player_modifier_runs_after_sample_before_pose_applied();
  test_tree_modifier_runs_after_sample_before_pose_applied();
  test_disabled_modifier_skipped();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("skeleton_modifier_timing_test: all passed\n");
  return 0;
}
