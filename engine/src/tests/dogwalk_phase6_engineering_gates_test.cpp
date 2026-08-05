#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_preview_controller.h"
#include "runtime/function/script/animation_frame.h"

#include <cmath>
#include <cstdio>

#include <glm/gtc/quaternion.hpp>

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

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = 1e-4f) {
  return float_near(a.x, b.x, eps) && float_near(a.y, b.y, eps) &&
         float_near(a.z, b.z, eps);
}

bool quat_near(const Blunder::Quat& a, const Blunder::Quat& b,
               float eps = 1e-4f) {
  const float dot = std::fabs(glm::dot(a, b));
  return dot > 1.0f - eps || dot < -1.0f + eps;
}

void decompose_bone_matrix(const Blunder::Mat4& matrix, Blunder::Vec3& position,
                           Blunder::Quat& rotation, Blunder::Vec3& scale) {
  position = Blunder::Vec3(matrix[3]);
  Blunder::Vec3 col0(matrix[0]);
  Blunder::Vec3 col1(matrix[1]);
  Blunder::Vec3 col2(matrix[2]);
  scale.x = glm::length(col0);
  scale.y = glm::length(col1);
  scale.z = glm::length(col2);
  if (scale.x > 1e-8f) {
    col0 /= scale.x;
  }
  if (scale.y > 1e-8f) {
    col1 /= scale.y;
  }
  if (scale.z > 1e-8f) {
    col2 /= scale.z;
  }
  const Blunder::Mat3 rot_mat(col0, col1, col2);
  rotation = glm::quat_cast(Blunder::Mat4(rot_mat));
}

bool object_transform_matches_bone(const Blunder::Object& object,
                                   const Blunder::Skeleton& skeleton,
                                   size_t bone_index) {
  const Blunder::Mat4 bone_global =
      skeleton.getBoneGlobalPoseMatrix(bone_index);
  Blunder::Vec3 expected_position;
  Blunder::Quat expected_rotation;
  Blunder::Vec3 expected_scale;
  decompose_bone_matrix(bone_global, expected_position, expected_rotation,
                        expected_scale);
  return vec3_near(object.getPosition(), expected_position) &&
         quat_near(object.getRotation(), expected_rotation) &&
         vec3_near(object.getScale(), expected_scale);
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

struct Phase6HarnessRig {
  Blunder::Object* host{nullptr};
  Blunder::Object* child{nullptr};
  Blunder::Skeleton* skeleton{nullptr};
  Blunder::AnimationPlayer* player{nullptr};
  Blunder::SkeletonPaperMouthModifier* mouth{nullptr};
  Blunder::SkeletonAttachModifier* attach{nullptr};
  Blunder::SkeletonLookAtModifier* look_at{nullptr};
  int jaw_index{-1};
  int snout_index{-1};
  int head_index{-1};

  bool setup() {
    using namespace Blunder;

    ObjectDB::clear();

    const ObjectId host_id = ObjectDB::create();
    const ObjectId child_id = ObjectDB::create();
    host = ObjectDB::get(host_id);
    child = ObjectDB::get(child_id);
    if (host == nullptr || child == nullptr) {
      return false;
    }
    child->setParent(host);

    skeleton = host->ensureSkeleton();
    player = host->ensureAnimationPlayer();

    const int hips = skeleton->addBone("Hips", -1);
    head_index = skeleton->addBone("Head", hips);
    jaw_index = skeleton->addBone("Jaw", head_index);
    snout_index = skeleton->addBone("Prop", hips);

    skeleton->setBoneRestLocal(static_cast<size_t>(head_index),
                               BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                             glm::identity<Quat>(), Vec3(1.0f)});
    skeleton->setBoneRestLocal(static_cast<size_t>(jaw_index),
                               BoneTransform{Vec3(0.0f, 0.0f, 0.2f),
                                             glm::identity<Quat>(), Vec3(1.0f)});
    skeleton->setBoneRestLocal(static_cast<size_t>(snout_index),
                               BoneTransform{Vec3(0.3f, 0.0f, 0.8f),
                                             glm::identity<Quat>(), Vec3(1.0f)});
    skeleton->resetPoseToRest();

    mouth = host->addSkeletonPaperMouthModifier();
    attach = host->addSkeletonAttachModifier();
    look_at = host->addSkeletonLookAtModifier();
    if (mouth == nullptr || attach == nullptr || look_at == nullptr) {
      return false;
    }

    mouth->setBoneName("Jaw");
    mouth->setOpenAmount(1.0f);

    attach->setBoneName("Prop");
    attach->setChildObjectId(child_id);

    look_at->setBoneName("Head");
    look_at->setTarget(Vec3(2.0f, 0.5f, 1.0f));

    const eastl::string guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
    AnimationClipData clip;
    clip.duration = 1.0f;
    clip.tracks.push_back(makeTranslationTrack(
        "Prop", AnimationInterpolation::Constant,
        {{0.0f, Vec3(0.6f, -0.1f, 0.4f)}, {1.0f, Vec3(0.6f, -0.1f, 0.4f)}}));
    player->setClipGuid("pose", guid);
    player->injectClipData(guid, clip);

    return true;
  }

  void teardown() {
    using namespace Blunder;
    ObjectDB::clear();
  }
};

/// Task 6.1: Play-path sample applies PaperMouth, Attach, and LookAt together.
void test_gate_play_three_modifier_product() {
  using namespace Blunder;

  ClassDB::initialize();

  Phase6HarnessRig rig;
  expect_true("harness rig setup", rig.setup());
  if (rig.host == nullptr || rig.skeleton == nullptr || rig.player == nullptr) {
    ClassDB::shutdown();
    return;
  }

  expect_true("three modifiers registered",
              rig.host->getSkeletonModifierCount() == 3);
  expect_true("slot 0 PaperMouth",
              eastl::string(rig.host->getSkeletonModifierAt(0)->getTypeName()) ==
                  "PaperMouth");
  expect_true("slot 1 Attach",
              eastl::string(rig.host->getSkeletonModifierAt(1)->getTypeName()) ==
                  "SkeletonAttachModifier");
  expect_true("slot 2 LookAt",
              eastl::string(rig.host->getSkeletonModifierAt(2)->getTypeName()) ==
                  "SkeletonLookAtModifier");

  const Quat jaw_rotation_before =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.jaw_index))
          .rotation;
  const Quat head_rotation_before =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.head_index))
          .rotation;
  const Vec3 child_position_before = rig.child->getPosition();

  expect_true("play pose clip", rig.player->play("pose"));

  const Quat jaw_rotation_after =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.jaw_index))
          .rotation;
  const Quat head_rotation_after =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.head_index))
          .rotation;

  expect_true("PaperMouth opens jaw after sample",
              std::fabs(glm::dot(jaw_rotation_before, jaw_rotation_after)) <
                  0.999f);
  expect_true("LookAt rotates head after sample",
              std::fabs(glm::dot(head_rotation_before, head_rotation_after)) <
                  0.999f);
  expect_true(
      "head aims at configured target",
      aim_dot_for_bone(*rig.skeleton, static_cast<size_t>(rig.head_index),
                       Vec3(2.0f, 0.5f, 1.0f)) > 0.95f);
  expect_true("Attach child position changed after sample",
              !vec3_near(rig.child->getPosition(), child_position_before));
  expect_true("Attach child follows sampled Snout world transform",
              object_transform_matches_bone(
                  *rig.child, *rig.skeleton,
                  static_cast<size_t>(rig.snout_index)));
  expect_true("Attach apply status",
              rig.attach->getLastApplyStatus() ==
                  SkeletonAttachApplyStatus::Applied);

  rig.teardown();
  ClassDB::shutdown();
}

/// Task 6.1: modifier chain order PaperMouth → Attach → LookAt is preserved
/// and Attach reads post-sample Snout pose (not rest-only).
void test_gate_chain_order_post_sample_attach() {
  using namespace Blunder;

  ClassDB::initialize();

  Phase6HarnessRig rig;
  expect_true("harness rig setup", rig.setup());
  if (rig.host == nullptr || rig.skeleton == nullptr || rig.player == nullptr) {
    ClassDB::shutdown();
    return;
  }

  rig.mouth->setOpenAmount(0.0f);
  rig.look_at->setEnabled(false);

  expect_true("play without look-at", rig.player->play("pose"));
  expect_true("Attach follows animated Prop translation",
              object_transform_matches_bone(
                  *rig.child, *rig.skeleton,
                  static_cast<size_t>(rig.snout_index)));

  const Quat head_rotation_sampled =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.head_index))
          .rotation;

  rig.look_at->setEnabled(true);
  rig.look_at->setTarget(Vec3(-1.0f, 2.0f, 0.5f));
  rig.host->applySkeletonModifiers(*rig.skeleton);

  expect_true("LookAt runs after sample when re-applied",
              aim_dot_for_bone(*rig.skeleton, static_cast<size_t>(rig.head_index),
                               Vec3(-1.0f, 2.0f, 0.5f)) > 0.95f);
  expect_true(
      "LookAt changed head rotation from sampled pose",
      std::fabs(glm::dot(head_rotation_sampled,
                         rig.skeleton
                             ->getBonePoseLocal(
                                 static_cast<size_t>(rig.head_index))
                             .rotation)) < 0.999f);

  rig.mouth->setOpenAmount(1.0f);
  const Quat jaw_before_mouth =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.jaw_index))
          .rotation;
  rig.host->applySkeletonModifiers(*rig.skeleton);
  const Quat jaw_after_mouth =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.jaw_index))
          .rotation;
  expect_true("PaperMouth still applies in chain after Attach",
              std::fabs(glm::dot(jaw_before_mouth, jaw_after_mouth)) < 0.999f);
  expect_true("Attach still applied after mouth re-apply",
              object_transform_matches_bone(
                  *rig.child, *rig.skeleton,
                  static_cast<size_t>(rig.snout_index)));

  rig.teardown();
  ClassDB::shutdown();
}

Blunder::Object* makePreviewHarnessObject(Blunder::Skeleton** out_skeleton,
                                         Blunder::Object** out_child) {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  if (host == nullptr || child == nullptr) {
    return nullptr;
  }
  child->setParent(host);

  Skeleton* skeleton = host->ensureSkeleton();
  AnimationPlayer* player = host->ensureAnimationPlayer();
  AnimationTree* tree = host->ensureAnimationTree();

  const int hips = skeleton->addBone("Hips", -1);
  const int head = skeleton->addBone("Head", hips);
  const int jaw = skeleton->addBone("Jaw", head);
  const int prop = skeleton->addBone("Prop", hips);
  (void)jaw;
  (void)prop;
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->setBoneRestLocal(static_cast<size_t>(jaw),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.2f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->setBoneRestLocal(static_cast<size_t>(prop),
                             BoneTransform{Vec3(0.3f, 0.0f, 0.8f),
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

  host->addSkeletonPaperMouthModifier();
  host->addSkeletonAttachModifier();
  host->addSkeletonLookAtModifier();

  if (out_skeleton != nullptr) {
    *out_skeleton = skeleton;
  }
  if (out_child != nullptr) {
    *out_child = child;
  }
  return host;
}

/// Task 6.1: Edit scrub key drives for all three modifiers without Behaviour Tick.
void test_gate_edit_scrub_three_modifiers() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* child = nullptr;
  Object* host = makePreviewHarnessObject(&skeleton, &child);
  expect_true("preview harness object", host != nullptr);
  if (host == nullptr || skeleton == nullptr || child == nullptr) {
    LifecycleDispatch::clear();
    return;
  }

  const int jaw = skeleton->findBoneIndex("Jaw");
  const int head = skeleton->findBoneIndex("Head");
  const int prop = skeleton->findBoneIndex("Prop");
  expect_true("jaw bone", jaw >= 0);
  expect_true("head bone", head >= 0);
  expect_true("prop bone", prop >= 0);

  AnimationPreviewController controller;
  controller.bindObject(host, "idle");
  expect_true("travel locomotion", controller.travel("Locomotion"));
  expect_true("activate tree", controller.setTreeActive(true));
  controller.setBlendSpaceScalar("Locomotion", 0.0f);

  expect_true("three modifiers in preview",
              controller.skeletonModifierCount() == 3);

  const size_t mouth_index = 0;
  const size_t attach_index = 1;
  const size_t look_at_index = 2;

  expect_true("scrub mouth closed",
              controller.setSkeletonPaperMouthOpenAmount(mouth_index, 0.0f));
  const Quat jaw_closed =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;

  expect_true("scrub mouth open",
              controller.setSkeletonPaperMouthOpenAmount(mouth_index, 1.0f));
  const Quat jaw_open =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  expect_true("mouth scrub opens jaw",
              std::fabs(glm::dot(jaw_closed, jaw_open)) < 0.999f);

  const Vec3 child_before_attach = child->getPosition();
  expect_true("scrub attach bone",
              controller.setSkeletonAttachBoneName(attach_index, "Prop"));
  expect_true("scrub attach child",
              controller.setSkeletonAttachChildObjectId(attach_index,
                                                        child->getId()));
  expect_true("attach scrub moves child",
              !vec3_near(child->getPosition(), child_before_attach));
  expect_true("attach scrub follows prop bone",
              object_transform_matches_bone(*child, *skeleton,
                                            static_cast<size_t>(prop)));

  const Vec3 target(1.0f, 1.0f, 1.0f);
  expect_true("scrub look-at target",
              controller.setSkeletonLookAtTarget(look_at_index, target));
  expect_true("look-at scrub aims head",
              aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target) >
                  0.95f);

  const Vec3 target_b(-2.0f, 0.5f, 1.0f);
  expect_true("scrub look-at target B",
              controller.setSkeletonLookAtTarget(look_at_index, target_b));
  expect_true("look-at scrub retargets head",
              aim_dot_for_bone(*skeleton, static_cast<size_t>(head),
                               target_b) > 0.95f);

  expect_true("no behaviour tick during three-modifier edit scrub",
              g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

}  // namespace

int main() {
  test_gate_play_three_modifier_product();
  test_gate_chain_order_post_sample_attach();
  test_gate_edit_scrub_three_modifiers();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_engineering_gates_test: all passed\n");
  return 0;
}
