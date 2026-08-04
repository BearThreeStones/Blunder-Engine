#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_preview_controller.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;

void on_tick(void* /*peer*/, float /*dt*/) { ++g_tick_calls; }

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

float aim_dot_for_bone(const Blunder::Skeleton& skeleton, size_t bone_index,
                       const Blunder::Vec3& target) {
  const Blunder::Mat4 head_global =
      skeleton.getBoneGlobalPoseMatrix(bone_index);
  const Blunder::Vec3 bone_pos(head_global[3]);
  const Blunder::Vec3 bone_forward = glm::normalize(
      Blunder::Vec3(head_global * Blunder::Vec4(0.0f, 1.0f, 0.0f, 0.0f)));
  const Blunder::Vec3 desired_forward = glm::normalize(target - bone_pos);
  return glm::dot(bone_forward, desired_forward);
}

bool set_look_at_target_via_classdb(Blunder::SkeletonLookAtModifier* modifier,
                                    const Blunder::Vec3& target) {
  using namespace Blunder;
  return ClassDB::setProperty(modifier, "SkeletonLookAtModifier", "target_x",
                              Variant(target.x)) &&
         ClassDB::setProperty(modifier, "SkeletonLookAtModifier", "target_y",
                              Variant(target.y)) &&
         ClassDB::setProperty(modifier, "SkeletonLookAtModifier", "target_z",
                              Variant(target.z));
}

bool get_look_at_target_via_classdb(const Blunder::SkeletonLookAtModifier* modifier,
                                    Blunder::Vec3& out_target) {
  using namespace Blunder;
  Variant x;
  Variant y;
  Variant z;
  if (!ClassDB::getProperty(modifier, "SkeletonLookAtModifier", "target_x", x) ||
      !ClassDB::getProperty(modifier, "SkeletonLookAtModifier", "target_y", y) ||
      !ClassDB::getProperty(modifier, "SkeletonLookAtModifier", "target_z", z)) {
    return false;
  }
  out_target = Vec3(x.asFloat(), y.asFloat(), z.asFloat());
  return true;
}

/// Task 1.1: LookAt ClassDB product — bone name and target independently
/// configurable; apply reads the configured min field set.
void test_look_at_product_configurable_bone_and_target() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("SkeletonLookAtModifier registered",
              ClassDB::hasClass("SkeletonLookAtModifier"));

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    ClassDB::shutdown();
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  const int hips = skeleton->addBone("Hips", -1);
  const int neck = skeleton->addBone("Neck", hips);
  const int head = skeleton->addBone("Head", neck);
  skeleton->setBoneRestLocal(static_cast<size_t>(neck),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.5f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  SkeletonLookAtModifier* look_at = object->addSkeletonLookAtModifier();
  expect_true("look-at product created", look_at != nullptr);
  if (look_at == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  expect_true("bone_name set via ClassDB",
              ClassDB::setProperty(look_at, "SkeletonLookAtModifier", "bone_name",
                                   Variant(eastl::string("Head"))));
  const Vec3 target_a(1.0f, 1.0f, 1.0f);
  expect_true("target set via ClassDB", set_look_at_target_via_classdb(look_at, target_a));

  Variant bone_name;
  expect_true("bone_name get via ClassDB",
              ClassDB::getProperty(look_at, "SkeletonLookAtModifier", "bone_name",
                                   bone_name));
  expect_true("bone_name round-trip", bone_name.asString() == "Head");

  Vec3 target_round_trip;
  expect_true("target get via ClassDB",
              get_look_at_target_via_classdb(look_at, target_round_trip));
  expect_true("target round-trip",
              float_near(target_round_trip.x, target_a.x) &&
                  float_near(target_round_trip.y, target_a.y) &&
                  float_near(target_round_trip.z, target_a.z));

  skeleton->resetPoseToRest();
  look_at->apply(*skeleton);
  const float aim_a =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_a);
  const Quat rotation_target_a =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("head aims at target A", aim_a > 0.95f);

  const Vec3 target_b(-2.0f, 0.5f, 1.0f);
  expect_true("target B set via ClassDB",
              set_look_at_target_via_classdb(look_at, target_b));
  skeleton->resetPoseToRest();
  look_at->apply(*skeleton);
  const float aim_b =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_b);
  const Quat rotation_target_b =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("head aims at target B", aim_b > 0.95f);
  expect_true("target change rotates bone",
              std::fabs(glm::dot(rotation_target_a, rotation_target_b)) <
                  0.999f);

  expect_true("bone_name switched to Neck via ClassDB",
              ClassDB::setProperty(look_at, "SkeletonLookAtModifier", "bone_name",
                                   Variant(eastl::string("Neck"))));
  skeleton->resetPoseToRest();
  const Quat head_rotation_before =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  look_at->apply(*skeleton);
  const Quat head_rotation_after =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  const float neck_aim =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(neck), target_b);
  expect_true("neck aims after bone change", neck_aim > 0.95f);
  expect_true("head rotation unchanged when aiming neck",
              std::fabs(glm::dot(head_rotation_before, head_rotation_after)) >
                  0.999f);

  ObjectDB::clear();
  ClassDB::shutdown();
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

Blunder::Object* makeLookAtPreviewObject(Blunder::Skeleton** out_skeleton) {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  if (object == nullptr) {
    return nullptr;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();

  const int hips = skeleton->addBone("Hips", -1);
  const int neck = skeleton->addBone("Neck", hips);
  const int head = skeleton->addBone("Head", neck);
  (void)head;
  skeleton->setBoneRestLocal(static_cast<size_t>(neck),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.5f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  constexpr const char* kIdleGuid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  constexpr const char* kWalkGuid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";

  AnimationClipData idle_clip;
  idle_clip.duration = 1.0f;
  idle_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  player->setClipGuid("idle", kIdleGuid);
  player->injectClipData(kIdleGuid, idle_clip);

  AnimationClipData walk_clip;
  walk_clip.duration = 1.0f;
  walk_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  player->setClipGuid("walk", kWalkGuid);
  player->injectClipData(kWalkGuid, walk_clip);

  tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
  tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
  tree->setStateBlendSpace("Locomotion", "Locomotion");

  if (out_skeleton != nullptr) {
    *out_skeleton = skeleton;
  }
  return object;
}

/// Task 1.2: Edit scrub LookAt via AnimationPreviewController without Behaviour Tick.
void test_edit_scrub_look_at_without_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* object = makeLookAtPreviewObject(&skeleton);
  expect_true("preview object", object != nullptr);
  if (object == nullptr) {
    LifecycleDispatch::clear();
    return;
  }

  const int head = skeleton->findBoneIndex("Head");
  const int neck = skeleton->findBoneIndex("Neck");
  expect_true("head bone", head >= 0);
  expect_true("neck bone", neck >= 0);

  SkeletonLookAtModifier* look_at = object->addSkeletonLookAtModifier();
  expect_true("look-at modifier", look_at != nullptr);
  if (look_at == nullptr) {
    ObjectDB::clear();
    LifecycleDispatch::clear();
    return;
  }

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  expect_true("travel locomotion", controller.travel("Locomotion"));
  const Quat head_sampled_rest =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("activate tree", controller.setTreeActive(true));
  controller.setBlendSpaceScalar("Locomotion", 0.0f);

  const size_t look_at_index = 0;
  expect_true("one modifier", controller.skeletonModifierCount() == 1);

  const Vec3 target_a(1.0f, 1.0f, 1.0f);
  expect_true("scrub target A",
              controller.setSkeletonLookAtTarget(look_at_index, target_a));
  const float aim_a =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_a);
  const Quat rotation_target_a =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("head aims at target A via edit scrub", aim_a > 0.95f);

  const Vec3 target_b(-2.0f, 0.5f, 1.0f);
  expect_true("scrub target B",
              controller.setSkeletonLookAtTarget(look_at_index, target_b));
  const float aim_b =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_b);
  const Quat rotation_target_b =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("head aims at target B via edit scrub", aim_b > 0.95f);
  expect_true("target scrub rotates bone",
              std::fabs(glm::dot(rotation_target_a, rotation_target_b)) <
                  0.999f);

  // Pre-bone-scrub baseline: head pose while LookAt still drives Head (target B).
  const Quat head_rotation_before_bone_scrub = rotation_target_b;
  expect_true("scrub bone to Neck",
              controller.setSkeletonLookAtBoneName(look_at_index, "Neck"));
  const float neck_aim =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(neck), target_b);
  expect_true("neck aims after bone scrub", neck_aim > 0.95f);
  const Quat head_rotation_after =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("head look-at released after bone scrub",
              std::fabs(glm::dot(head_rotation_before_bone_scrub,
                                 head_rotation_after)) < 0.999f);
  expect_true("head unchanged when aiming neck via edit scrub",
              std::fabs(glm::dot(head_sampled_rest, head_rotation_after)) >
                  0.999f);

  expect_true("no behaviour tick during look-at edit scrub", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

}  // namespace

int main() {
  test_look_at_product_configurable_bone_and_target();
  test_edit_scrub_look_at_without_behaviour_tick();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_look_at_product_test: all passed\n");
  return 0;
}
