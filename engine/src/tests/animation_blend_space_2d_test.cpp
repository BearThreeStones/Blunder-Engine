#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
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
               float eps = 1e-3f) {
  return float_near(a.x, b.x, eps) && float_near(a.y, b.y, eps) &&
         float_near(a.z, b.z, eps);
}

Blunder::AnimationTrack makeTranslationTrack(
    const char* bone,
    std::initializer_list<std::pair<float, Blunder::Vec3>> keys) {
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = Blunder::AnimationChannel::Translation;
  track.interpolation = Blunder::AnimationInterpolation::Constant;
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

Blunder::AnimationClipData makeConstClip(const Blunder::Vec3& translation,
                                         float duration) {
  Blunder::AnimationClipData clip;
  clip.duration = duration;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", {{0.0f, translation}, {duration, translation}}));
  return clip;
}

struct Fixture {
  Blunder::Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  Blunder::AnimationPlayer player;
  Blunder::AnimationTree tree;

  Fixture() {
    player.bindSamplingSkeleton(&skeleton);
    tree.bindAnimationPlayer(&player);
    tree.bindSamplingSkeleton(&skeleton);
  }

  void inject(const char* name, const char* guid,
              const Blunder::AnimationClipData& clip) {
    player.setClipGuid(name, guid);
    player.injectClipData(guid, clip);
  }
};

void test_blend_space2d_interior_barycentric() {
  using namespace Blunder;
  Fixture f;

  f.inject("a", "11111111-1111-1111-1111-111111111111",
           makeConstClip(Vec3(0.0f, 0.0f, 0.0f), 1.0f));
  f.inject("b", "22222222-2222-2222-2222-222222222222",
           makeConstClip(Vec3(10.0f, 0.0f, 0.0f), 1.0f));
  f.inject("c", "33333333-3333-3333-3333-333333333333",
           makeConstClip(Vec3(0.0f, 10.0f, 0.0f), 1.0f));

  expect_true("add a", f.tree.addBlendSpace2DPoint("Locomotion2D", "a", 0.0f, 0.0f));
  expect_true("add b", f.tree.addBlendSpace2DPoint("Locomotion2D", "b", 1.0f, 0.0f));
  expect_true("add c", f.tree.addBlendSpace2DPoint("Locomotion2D", "c", 0.0f, 1.0f));
  expect_true("set base 2d", f.tree.setBaseBlendSpace2DNode("Locomotion2D"));

  // Centroid of the triangle → equal 1/3 weights → (10/3, 10/3, 0)
  f.tree.setBlendSpace2DParam("Locomotion2D", 1.0f / 3.0f, 1.0f / 3.0f);
  expect_true("activate", f.tree.setActive(true));
  expect_true(
      "centroid barycentric pose",
      vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                Vec3(10.0f / 3.0f, 10.0f / 3.0f, 0.0f)));
}

void test_blend_space2d_param_by_node_name() {
  using namespace Blunder;
  Fixture f;

  f.inject("idle", "11111111-1111-1111-1111-111111111111",
           makeConstClip(Vec3(0.0f, 0.0f, 0.0f), 1.0f));
  f.inject("walk", "22222222-2222-2222-2222-222222222222",
           makeConstClip(Vec3(8.0f, 0.0f, 0.0f), 1.0f));
  f.inject("run", "33333333-3333-3333-3333-333333333333",
           makeConstClip(Vec3(0.0f, 8.0f, 0.0f), 1.0f));
  f.inject("other", "44444444-4444-4444-4444-444444444444",
           makeConstClip(Vec3(100.0f, 0.0f, 0.0f), 1.0f));

  f.tree.addBlendSpace2DPoint("Locomotion2D", "idle", 0.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("Locomotion2D", "walk", 1.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("Locomotion2D", "run", 0.0f, 1.0f);
  f.tree.addBlendSpace2DPoint("UpperBody2D", "other", 0.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("UpperBody2D", "walk", 1.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("UpperBody2D", "run", 0.0f, 1.0f);

  f.tree.setBaseBlendSpace2DNode("Locomotion2D");
  f.tree.setBlendSpace2DParam("UpperBody2D", 1.0f, 0.0f);
  f.tree.setBlendSpace2DParam("Locomotion2D", 0.0f, 0.0f);
  expect_true("activate", f.tree.setActive(true));

  expect_true("only locomotion param affects base",
              vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                        Vec3(0.0f, 0.0f, 0.0f)));

  const BlendSpace2DParam loco = f.tree.getBlendSpace2DParam("Locomotion2D");
  const BlendSpace2DParam upper = f.tree.getBlendSpace2DParam("UpperBody2D");
  expect_true("loco param x", float_near(loco.x, 0.0f));
  expect_true("loco param y", float_near(loco.y, 0.0f));
  expect_true("upper param x", float_near(upper.x, 1.0f));
  expect_true("upper param y", float_near(upper.y, 0.0f));
}

void test_state_machine_blend_space2d_playback() {
  using namespace Blunder;
  Fixture f;

  f.inject("idle", "11111111-1111-1111-1111-111111111111",
           makeConstClip(Vec3(0.0f, 0.0f, 0.0f), 1.0f));
  f.inject("walk", "22222222-2222-2222-2222-222222222222",
           makeConstClip(Vec3(10.0f, 0.0f, 0.0f), 1.0f));
  f.inject("run", "33333333-3333-3333-3333-333333333333",
           makeConstClip(Vec3(0.0f, 10.0f, 0.0f), 1.0f));
  f.inject("pose", "44444444-4444-4444-4444-444444444444",
           makeConstClip(Vec3(3.0f, 0.0f, 0.0f), 1.0f));

  f.tree.addBlendSpace2DPoint("Locomotion2D", "idle", 0.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("Locomotion2D", "walk", 1.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("Locomotion2D", "run", 0.0f, 1.0f);
  f.tree.setBlendSpace2DParam("Locomotion2D", 1.0f, 0.0f);
  expect_true("state 2d",
              f.tree.setStateBlendSpace2D("Move2D", "Locomotion2D"));
  f.tree.setStateClip("Pose", "pose");

  expect_true("activate", f.tree.setActive(true));
  expect_true("travel pose", f.tree.travel("Pose"));
  expect_true("clip state",
              vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                        Vec3(3.0f, 0.0f, 0.0f)));

  expect_true("travel 2d", f.tree.travel("Move2D"));
  expect_true("2d state pose",
              vec3_near(f.skeleton.getBonePoseLocal(0).translation,
                        Vec3(10.0f, 0.0f, 0.0f)));
}

void test_blend_space2d_dominant_clip_tie_break() {
  using namespace Blunder;
  Fixture f;

  // Different durations so dominant clip length is observable.
  f.inject("a", "11111111-1111-1111-1111-111111111111",
           makeConstClip(Vec3(0.0f, 0.0f, 0.0f), 1.0f));
  f.inject("b", "22222222-2222-2222-2222-222222222222",
           makeConstClip(Vec3(10.0f, 0.0f, 0.0f), 2.0f));
  f.inject("c", "33333333-3333-3333-3333-333333333333",
           makeConstClip(Vec3(0.0f, 10.0f, 0.0f), 3.0f));

  f.tree.addBlendSpace2DPoint("Locomotion2D", "a", 0.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("Locomotion2D", "b", 1.0f, 0.0f);
  f.tree.addBlendSpace2DPoint("Locomotion2D", "c", 0.0f, 1.0f);
  f.tree.setBaseBlendSpace2DNode("Locomotion2D");

  // Near b → dominant b (duration 2).
  f.tree.setBlendSpace2DParam("Locomotion2D", 0.9f, 0.05f);
  expect_true("activate", f.tree.setActive(true));
  expect_true("dominant near b length",
              float_near(f.player.getClipLength(), 2.0f, 1e-4f));

  // Exact centroid: equal weights → lowest index (a, duration 1).
  f.tree.setBlendSpace2DParam("Locomotion2D", 1.0f / 3.0f, 1.0f / 3.0f);
  f.tree.sampleBoundSkeleton();
  expect_true("tie-break lowest index length",
              float_near(f.player.getClipLength(), 1.0f, 1e-4f));
}

}  // namespace

int main() {
  test_blend_space2d_interior_barycentric();
  test_blend_space2d_param_by_node_name();
  test_state_machine_blend_space2d_playback();
  test_blend_space2d_dominant_clip_tie_break();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
