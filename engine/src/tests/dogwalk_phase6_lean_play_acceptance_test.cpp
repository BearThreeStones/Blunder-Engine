#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
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
               float eps = 1e-3f) {
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

struct LeanRig {
  Blunder::Object* host{nullptr};
  Blunder::Object* child{nullptr};
  Blunder::Skeleton* skeleton{nullptr};
  Blunder::AnimationPlayer* player{nullptr};
  Blunder::SkeletonPaperMouthModifier* mouth{nullptr};
  Blunder::SkeletonAttachModifier* attach{nullptr};
  Blunder::SkeletonLookAtModifier* look_at{nullptr};
  int jaw_index{-1};
  int head_index{-1};
  int prop_index{-1};

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
    prop_index = skeleton->addBone("Prop", hips);

    skeleton->setBoneRestLocal(static_cast<size_t>(head_index),
                               BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                             glm::identity<Quat>(), Vec3(1.0f)});
    skeleton->setBoneRestLocal(static_cast<size_t>(jaw_index),
                               BoneTransform{Vec3(0.0f, 0.0f, 0.2f),
                                             glm::identity<Quat>(), Vec3(1.0f)});
    skeleton->setBoneRestLocal(static_cast<size_t>(prop_index),
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

  void teardown() { Blunder::ObjectDB::clear(); }
};

/// Lean Play: visible mouth open, child follow, aim change under Play tick.
void test_lean_play() {
  using namespace Blunder;

  LeanRig rig;
  expect_true("lean play rig", rig.setup());
  if (rig.host == nullptr || rig.skeleton == nullptr || rig.player == nullptr) {
    return;
  }

  const Quat jaw_before =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.jaw_index))
          .rotation;
  const Vec3 child_before = rig.child->getPosition();

  expect_true("lean play start", rig.player->play("pose"));
  tickObjectAnimationPlayFrame(rig.host, 0.25f, /*play_paused=*/false);

  const Quat jaw_after =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.jaw_index))
          .rotation;
  expect_true("lean play mouth open",
              std::fabs(glm::dot(jaw_before, jaw_after)) < 0.999f);
  expect_true("lean play child moved", !vec3_near(rig.child->getPosition(), child_before));
  expect_true("lean play child follows prop",
              object_transform_matches_bone(
                  *rig.child, *rig.skeleton,
                  static_cast<size_t>(rig.prop_index)));
  expect_true(
      "lean play head aims",
      aim_dot_for_bone(*rig.skeleton, static_cast<size_t>(rig.head_index),
                       Vec3(2.0f, 0.5f, 1.0f)) > 0.95f);

  const Quat head_before_retarget =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.head_index))
          .rotation;
  rig.look_at->setTarget(Vec3(-1.0f, 2.0f, 0.5f));
  tickObjectAnimationPlayFrame(rig.host, 0.001f, /*play_paused=*/false);
  const Quat head_after_retarget =
      rig.skeleton->getBonePoseLocal(static_cast<size_t>(rig.head_index))
          .rotation;
  expect_true(
      "lean play aim retargets",
      aim_dot_for_bone(*rig.skeleton, static_cast<size_t>(rig.head_index),
                       Vec3(-1.0f, 2.0f, 0.5f)) > 0.95f);
  expect_true("lean play aim changed",
              std::fabs(glm::dot(head_before_retarget, head_after_retarget)) <
                  0.999f);

  rig.teardown();
}

/// Lean Edit: scrub mouth open, child follow, look-at retarget without Behaviour Tick.
void test_lean_edit() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  const ObjectId host_id = ObjectDB::create();
  const ObjectId child_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  Object* child = ObjectDB::get(child_id);
  expect_true("lean edit host", host != nullptr && child != nullptr);
  if (host == nullptr || child == nullptr) {
    LifecycleDispatch::clear();
    return;
  }
  child->setParent(host);

  Skeleton* skeleton = host->ensureSkeleton();
  AnimationPlayer* player = host->ensureAnimationPlayer();
  AnimationTree* tree = host->ensureAnimationTree();
  const int hips = skeleton->addBone("Hips", -1);
  const int head = skeleton->addBone("Head", hips);
  const int jaw = skeleton->addBone("Jaw", head);
  const int prop = skeleton->addBone("Prop", hips);
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

  const eastl::string idle_guid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  const eastl::string walk_guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  AnimationClipData idle_clip;
  idle_clip.duration = 1.0f;
  idle_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  player->setClipGuid("idle", idle_guid);
  player->injectClipData(idle_guid, idle_clip);

  AnimationClipData walk_clip;
  walk_clip.duration = 1.0f;
  walk_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  player->setClipGuid("walk", walk_guid);
  player->injectClipData(walk_guid, walk_clip);

  tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
  tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
  tree->setStateBlendSpace("Locomotion", "Locomotion");

  host->addSkeletonPaperMouthModifier();
  host->addSkeletonAttachModifier();
  host->addSkeletonLookAtModifier();

  AnimationPreviewController controller;
  controller.bindObject(host, "idle");
  expect_true("lean edit travel", controller.travel("Locomotion"));
  expect_true("lean edit tree active", controller.setTreeActive(true));
  controller.setBlendSpaceScalar("Locomotion", 0.0f);
  expect_true("lean edit three modifiers", controller.skeletonModifierCount() == 3);

  expect_true("lean edit mouth closed",
              controller.setSkeletonPaperMouthOpenAmount(0, 0.0f));
  const Quat jaw_closed =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;

  expect_true("lean edit mouth open",
              controller.setSkeletonPaperMouthOpenAmount(0, 1.0f));
  const Quat jaw_open =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  expect_true("lean edit mouth visible",
              std::fabs(glm::dot(jaw_closed, jaw_open)) < 0.999f);

  const Vec3 child_before = child->getPosition();
  expect_true("lean edit attach bone",
              controller.setSkeletonAttachBoneName(1, "Prop"));
  expect_true("lean edit attach child",
              controller.setSkeletonAttachChildObjectId(1, child->getId()));
  expect_true("lean edit child moved", !vec3_near(child->getPosition(), child_before));
  expect_true("lean edit child follows prop",
              object_transform_matches_bone(*child, *skeleton,
                                            static_cast<size_t>(prop)));

  const Vec3 target_a(1.0f, 1.0f, 1.0f);
  expect_true("lean edit look-at A",
              controller.setSkeletonLookAtTarget(2, target_a));
  expect_true("lean edit aim A",
              aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_a) >
                  0.95f);
  const Quat head_before_aim =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;

  const Vec3 target_b(-2.0f, 0.5f, 1.0f);
  expect_true("lean edit look-at B",
              controller.setSkeletonLookAtTarget(2, target_b));
  expect_true("lean edit aim B",
              aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_b) >
                  0.95f);
  const Quat head_after_aim =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("lean edit aim changed",
              std::fabs(glm::dot(head_before_aim, head_after_aim)) < 0.999f);
  expect_true("lean edit no behaviour tick", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

}  // namespace

int main() {
  test_lean_play();
  test_lean_edit();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_lean_play_acceptance_test: all passed\n");
  return 0;
}
