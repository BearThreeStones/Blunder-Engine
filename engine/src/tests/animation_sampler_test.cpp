#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sampler.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"

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

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = 1e-4f) {
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

Blunder::Skeleton makeSingleBoneSkeleton(const char* bone_name) {
  Blunder::Skeleton skeleton;
  skeleton.addBone(bone_name, -1);
  return skeleton;
}

void test_constant_hold_mid_interval() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationClipData clip;
  clip.duration = 2.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(1.0f, 0.0f, 0.0f)}, {2.0f, Vec3(9.0f, 0.0f, 0.0f)}}));

  sampleClipOntoSkeleton(skeleton, clip, 1.0f);
  expect_true("constant holds preceding key",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(1.0f, 0.0f, 0.0f)));
}

void test_linear_midpoint() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationClipData clip;
  clip.duration = 2.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {2.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  sampleClipOntoSkeleton(skeleton, clip, 1.0f);
  expect_true("linear midpoint",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(5.0f, 0.0f, 0.0f)));
}

void test_missing_bone_ignored_safely() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Known");
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Missing", AnimationInterpolation::Linear,
      {{0.0f, Vec3(4.0f, 0.0f, 0.0f)}, {1.0f, Vec3(8.0f, 0.0f, 0.0f)}}));

  sampleClipOntoSkeleton(skeleton, clip, 0.5f);
  expect_true("known bone stays at rest",
              vec3_near(skeleton.getBonePoseLocal(0).translation, Vec3(0.0f)));
}

void test_hard_cut_and_sample_via_player() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  player.bindSamplingSkeleton(&skeleton);

  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";

  AnimationClipData idle;
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(1.0f, 0.0f, 0.0f)}, {1.0f, Vec3(1.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.duration = 2.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {2.0f, Vec3(8.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);

  expect_true("play idle", player.play("idle"));
  player.advance(0.4f);
  expect_true("idle pose sampled",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(1.0f, 0.0f, 0.0f)));

  expect_true("hard cut to walk", player.play("walk"));
  player.advance(1.0f);
  expect_true("walk pose sampled at midpoint",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(4.0f, 0.0f, 0.0f)));
}

void test_no_skeleton_binding_is_no_op() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(3.0f, 0.0f, 0.0f)}, {1.0f, Vec3(3.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("clip", guid);
  player.injectClipData(guid, clip);
  expect_true("play without skeleton", player.play("clip"));
  player.advance(0.5f);
  expect_true("still playing", player.isPlaying());
}

void test_object_colocated_sampling() {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  expect_true("components exist", skeleton != nullptr && player != nullptr);

  skeleton->addBone("Hips", -1);
  const eastl::string guid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(2.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("move", guid);
  player->injectClipData(guid, clip);
  expect_true("play on object", player->play("move"));
  player->advance(0.5f);
  expect_true("object samples co-located skeleton",
              vec3_near(skeleton->getBonePoseLocal(0).translation,
                        Vec3(1.0f, 0.0f, 0.0f)));

  ObjectDB::clear();
}

}  // namespace

int main() {
  test_constant_hold_mid_interval();
  test_linear_midpoint();
  test_missing_bone_ignored_safely();
  test_hard_cut_and_sample_via_player();
  test_no_skeleton_binding_is_no_op();
  test_object_colocated_sampling();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
