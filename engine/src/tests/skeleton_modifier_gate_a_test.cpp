#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/object/skeleton_modifier_test_double.h"
#include "runtime/function/script/animation_frame.h"

#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>

namespace {

int g_failures = 0;
int g_chain_order = 0;
int g_modifier_a_order = 0;
int g_modifier_b_order = 0;
float g_bone_x_after_modifier_a = -1.0f;

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

void reset_chain_spies() {
  g_chain_order = 0;
  g_modifier_a_order = 0;
  g_modifier_b_order = 0;
  g_bone_x_after_modifier_a = -1.0f;
}

void modifier_a_add_ten(Blunder::Skeleton& skeleton, void* /*userdata*/) {
  g_modifier_a_order = ++g_chain_order;
  if (skeleton.getBoneCount() > 0) {
    g_bone_x_after_modifier_a = skeleton.getBonePoseLocal(0).translation.x;
    Blunder::BoneTransform pose = skeleton.getBonePoseLocal(0);
    pose.translation.x += 10.0f;
    skeleton.setBonePoseLocal(0, pose);
  }
}

void modifier_b_add_twenty(Blunder::Skeleton& skeleton, void* /*userdata*/) {
  g_modifier_b_order = ++g_chain_order;
  if (skeleton.getBoneCount() > 0) {
    Blunder::BoneTransform pose = skeleton.getBonePoseLocal(0);
    pose.translation.x += 20.0f;
    skeleton.setBonePoseLocal(0, pose);
  }
}

/// Task 1.2: modifiers A then B run in registration order on the same Object.
void test_ordered_modifier_chain_on_same_object() {
  using namespace Blunder;

  ObjectDB::clear();
  reset_chain_spies();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  skeleton->addBone("Hips", -1);

  SkeletonModifier* modifier_a = object->addSkeletonModifier();
  SkeletonModifier* modifier_b = object->addSkeletonModifier();
  expect_true("modifier A created", modifier_a != nullptr);
  expect_true("modifier B created", modifier_b != nullptr);
  if (modifier_a == nullptr || modifier_b == nullptr) {
    return;
  }
  modifier_a->setApplyFn(modifier_a_add_ten, nullptr);
  modifier_b->setApplyFn(modifier_b_add_twenty, nullptr);

  const eastl::string guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  AnimationClipData clip;
  clip.duration = 2.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {2.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("move", guid);
  player->injectClipData(guid, clip);
  expect_true("play", player->play("move"));

  reset_chain_spies();
  tickObjectAnimationPlayFrame(object, 1.0f, /*play_paused=*/false);

  expect_true("modifier A ran", g_modifier_a_order > 0);
  expect_true("modifier B ran", g_modifier_b_order > 0);
  expect_true("A before B", g_modifier_a_order < g_modifier_b_order);
  expect_true("A sees sampled pose", float_near(g_bone_x_after_modifier_a, 5.0f));
  expect_true("final pose stacks both offsets",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 35.0f));

  ObjectDB::clear();
}

/// Task 1.3: subclass extension point via SkeletonModifierTestDouble.
void test_extension_point_test_double_modifier() {
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
  skeleton->addBone("Hips", -1);

  int order_counter = 0;
  auto test_double = eastl::make_unique<SkeletonModifierTestDouble>();
  test_double->setOrderCounter(&order_counter);
  test_double->setBoneXOffset(7.0f);
  SkeletonModifierTestDouble* raw_double = static_cast<SkeletonModifierTestDouble*>(
      object->addSkeletonModifier(eastl::move(test_double)));
  expect_true("test double stored", raw_double != nullptr);
  if (raw_double == nullptr) {
    return;
  }

  const eastl::string guid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(4.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("move", guid);
  player->injectClipData(guid, clip);
  expect_true("play", player->play("move"));

  raw_double->resetSpy();
  order_counter = 0;
  tickObjectAnimationPlayFrame(object, 0.5f, /*play_paused=*/false);

  expect_true("test double invoked", raw_double->getApplyCount() == 1);
  expect_true("test double recorded order", raw_double->getRecordedOrder() == 1);
  expect_true("test double mutated pose",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 9.0f));

  ObjectDB::clear();
}

/// Task 1.4: LookAt sample rotates a bone toward a world target post-sample.
void test_look_at_modifier_produces_visible_post_pose_change() {
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
  const int hips = skeleton->addBone("Hips", -1);
  const int head = skeleton->addBone("Head", hips);
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  auto look_at = eastl::make_unique<SkeletonLookAtModifier>();
  look_at->setBoneName("Head");
  look_at->setTarget(Vec3(1.0f, 1.0f, 1.0f));
  object->addSkeletonModifier(eastl::move(look_at));

  const eastl::string guid = "cccccccc-cccc-cccc-cccc-cccccccccccc";
  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  player->setClipGuid("idle", guid);
  player->injectClipData(guid, clip);

  const Quat head_rotation_before =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("play", player->play("idle"));
  const Quat head_rotation_after =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;

  const Mat4 head_global = skeleton->getBoneGlobalPoseMatrix(static_cast<size_t>(head));
  const Vec3 head_pos(head_global[3]);
  const Vec3 head_forward = glm::normalize(Vec3(head_global * Vec4(0.0f, 1.0f, 0.0f, 0.0f)));
  const Vec3 desired_forward = glm::normalize(Vec3(1.0f, 1.0f, 1.0f) - head_pos);

  const float rotation_similarity =
      std::fabs(glm::dot(head_rotation_before, head_rotation_after));
  expect_true("look-at changed head rotation", rotation_similarity < 0.999f);
  expect_true("head aims toward target",
              glm::dot(head_forward, desired_forward) > 0.95f);

  ObjectDB::clear();
}

void bindPoseClip(Blunder::AnimationPlayer& player, const char* clip_name,
                  const char* guid, float duration,
                  const Blunder::Vec3& translation) {
  using namespace Blunder;
  eastl::string guid_str(guid);
  AnimationClipData clip;
  clip.name = clip_name;
  clip.duration = duration;
  clip.tracks.push_back(makeTranslationTrack(
      "Leg", AnimationInterpolation::Constant,
      {{0.0f, translation}, {duration, translation}}));
  player.setClipGuid(clip_name, guid_str);
  player.injectClipData(guid_str, clip);
}

/// Task 1.5: Add2 remains in-tree additive; SkeletonModifier is post-pose and distinct.
void test_add2_remains_distinct_from_skeleton_modifier() {
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
  AnimationTree* tree = object->ensureAnimationTree();

  const int hips = skeleton->addBone("Hips", -1);
  skeleton->addBone("Leg", hips);
  skeleton->resetPoseToRest();

  bindPoseClip(*player, "idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 1.0f,
               Vec3(0.0f, 0.0f, 0.0f));
  bindPoseClip(*player, "turn_add", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 1.0f,
               Vec3(0.0f, 2.0f, 0.0f));

  tree->setSampleClipName("idle");
  tree->setAdd2ClipName("turn_add");
  tree->setActive(true);
  tree->setAdd2Weight(0.5f);

  tree->sampleBoundSkeleton();
  expect_true("add2 affects leg without modifier",
              float_near(skeleton->getBonePoseLocal(1).translation.y, 1.0f));

  SkeletonModifier* hips_modifier = object->addSkeletonModifier();
  expect_true("modifier created", hips_modifier != nullptr);
  if (hips_modifier == nullptr) {
    return;
  }
  hips_modifier->setApplyFn(
      [](Skeleton& skel, void*) {
        if (skel.getBoneCount() > 0) {
          BoneTransform pose = skel.getBonePoseLocal(0);
          pose.translation.x += 50.0f;
          skel.setBonePoseLocal(0, pose);
        }
      },
      nullptr);

  tree->sampleBoundSkeleton();
  expect_true("add2 still applied with modifier",
              float_near(skeleton->getBonePoseLocal(1).translation.y, 1.0f));
  expect_true("modifier applied after add2 sample",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 50.0f));

  hips_modifier->setEnabled(false);
  skeleton->resetPoseToRest();
  tree->sampleBoundSkeleton();
  expect_true("disabled modifier does not offset hips",
              float_near(skeleton->getBonePoseLocal(0).translation.x, 0.0f));
  expect_true("add2 unaffected by modifier toggle",
              float_near(skeleton->getBonePoseLocal(1).translation.y, 1.0f));

  expect_true("add2 API separate from modifier count",
              tree->getAdd2Weight() == 0.5f && object->getSkeletonModifierCount() == 1);

  ObjectDB::clear();
}

}  // namespace

int main() {
  test_ordered_modifier_chain_on_same_object();
  test_extension_point_test_double_modifier();
  test_look_at_modifier_produces_visible_post_pose_change();
  test_add2_remains_distinct_from_skeleton_modifier();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("skeleton_modifier_gate_a_test: all passed\n");
  return 0;
}
