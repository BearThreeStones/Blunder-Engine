#include "runtime/core/object/animation_pipeline.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/function/scene/gpu_skinning.h"
#include "runtime/resource/asset/mesh_skin_data.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_pose_applied_count = 0;
bool g_pose_saw_valid_palette = false;
float g_pose_head_skin_ty = -999.0f;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) < eps;
}

bool mat4_near(const Blunder::Mat4& a, const Blunder::Mat4& b,
               float eps = 1e-3f) {
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      if (std::fabs(a[c][r] - b[c][r]) > eps) {
        return false;
      }
    }
  }
  return true;
}

Blunder::AnimationClipData make_translate_clip(const char* name,
                                               const char* bone,
                                               const Blunder::Vec3& translation) {
  Blunder::AnimationClipData clip;
  clip.name = name;
  clip.duration = 1.0f;
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = Blunder::AnimationChannel::Translation;
  track.interpolation = Blunder::AnimationInterpolation::Linear;
  Blunder::AnimationKeyframe key;
  key.time = 0.0f;
  key.value = {translation.x, translation.y, translation.z};
  track.keys.push_back(key);
  clip.tracks.push_back(track);
  return clip;
}

void on_pose_applied(Blunder::AnimationPlayer& player, void* /*userdata*/) {
  ++g_pose_applied_count;
  Blunder::Object* object = Blunder::ObjectDB::get(player.getOwnerObjectId());
  if (object == nullptr || object->getSkeleton() == nullptr) {
    return;
  }
  Blunder::Skeleton* skeleton = object->getSkeleton();
  g_pose_saw_valid_palette = skeleton->hasValidPoseBuffers();
  if (skeleton->getBoneCount() > 0) {
    const int head = skeleton->findBoneIndex("Head");
    if (head >= 0) {
      g_pose_head_skin_ty =
          skeleton->getBoneSkinMatrix(static_cast<size_t>(head))[3].y;
    }
  }
}

void test_single_clip_local_global_palette() {
  using namespace Blunder;
  ObjectDB::clear();
  g_pose_applied_count = 0;
  g_pose_saw_valid_palette = false;

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  const int root = skeleton->addBone("Root", -1);
  const int head = skeleton->addBone("Head", root);
  BoneTransform head_rest;
  head_rest.translation = Vec3(0.0f, 1.0f, 0.0f);
  skeleton->setBoneRestLocal(static_cast<size_t>(head), head_rest);
  skeleton->setBoneInverseBind(static_cast<size_t>(head),
                               glm::translate(Mat4(1.0f), Vec3(0.0f, -1.0f, 0.0f)));
  skeleton->resetPoseToRest();

  AnimationPlayer* player = object->ensureAnimationPlayer();
  player->injectClipData("clip-a",
                         make_translate_clip("idle", "Head", Vec3(0.0f, 2.0f, 0.0f)));
  player->setClipGuid("idle", "clip-a");
  player->addPoseAppliedListener(&on_pose_applied, nullptr);
  expect_true("play", player->play("idle"));
  // beginClip already sampled; ensure finalize path exercised once more.
  player->sampleOntoSkeleton(*skeleton);

  expect_true("pose applied", g_pose_applied_count >= 1);
  expect_true("palette valid at PoseApplied", g_pose_saw_valid_palette);
  expect_true("buffers valid", skeleton->hasValidPoseBuffers());

  const Mat4 global = skeleton->getBoneGlobalPoseMatrix(static_cast<size_t>(head));
  expect_true("global y from clip", float_near(global[3].y, 2.0f));
  const Mat4 skin = skeleton->getBoneSkinMatrix(static_cast<size_t>(head));
  const Mat4 expected = global * skeleton->getBoneInverseBind(static_cast<size_t>(head));
  expect_true("palette = global * ib", mat4_near(skin, expected));
}

void test_dual_slot_pose_applied_after_palette() {
  using namespace Blunder;
  ObjectDB::clear();
  g_pose_applied_count = 0;
  g_pose_saw_valid_palette = false;

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  Skeleton* skeleton = object->ensureSkeleton();
  skeleton->addBone("Root", -1);
  const int head = skeleton->addBone("Head", 0);
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 1.0f, 0.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  AnimationPlayer* player = object->ensureAnimationPlayer();
  player->injectClipData("c0", make_translate_clip("a", "Head", Vec3(0.0f, 1.0f, 0.0f)));
  player->injectClipData("c1", make_translate_clip("b", "Head", Vec3(0.0f, 3.0f, 0.0f)));
  player->setClipGuid("a", "c0");
  player->setClipGuid("b", "c1");
  player->setSlot(0, "a");
  player->setSlot(1, "b");
  player->setBlendWeight(0.5f);
  player->addPoseAppliedListener(&on_pose_applied, nullptr);
  expect_true("play for dual", player->play("a"));
  // Re-apply slots after play keeps dual-track sample path.
  player->setSlot(0, "a");
  player->setSlot(1, "b");
  player->setBlendWeight(0.5f);
  player->sampleOntoSkeleton(*skeleton);

  expect_true("dual pose applied", g_pose_applied_count >= 1);
  expect_true("dual palette valid", g_pose_saw_valid_palette);
  const float y =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).translation.y;
  expect_true("blended local y", float_near(y, 2.0f, 0.05f));
}

void test_look_at_world_target_and_palette() {
  using namespace Blunder;
  ObjectDB::clear();
  g_pose_applied_count = 0;

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  object->setPosition(Vec3(10.0f, 0.0f, 0.0f));

  Skeleton* skeleton = object->ensureSkeleton();
  const int root = skeleton->addBone("Root", -1);
  const int head = skeleton->addBone("Head", root);
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 1.0f, 0.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->setBoneInverseBind(static_cast<size_t>(head), Mat4(1.0f));
  skeleton->resetPoseToRest();

  SkeletonLookAtModifier* look_at = object->addSkeletonLookAtModifier();
  look_at->setBoneName("Head");
  // World-space point: object at x=10, bone model at (0,1,0) → world ~(10,1,0).
  // Aim toward world (10, 1, 5) ⇒ model-space forward +Z-ish relative aim.
  look_at->setTarget(Vec3(10.0f, 1.0f, 5.0f));

  AnimationPlayer* player = object->ensureAnimationPlayer();
  player->injectClipData("c", make_translate_clip("idle", "Head", Vec3(0.0f, 1.0f, 0.0f)));
  player->setClipGuid("idle", "c");
  player->addPoseAppliedListener(&on_pose_applied, nullptr);
  player->play("idle");
  player->sampleOntoSkeleton(*skeleton);

  expect_true("lookat pose applied", g_pose_applied_count >= 1);
  expect_true("lookat buffers valid", skeleton->hasValidPoseBuffers());

  const Mat4 global = skeleton->getBoneGlobalPoseMatrix(static_cast<size_t>(head));
  const Vec3 bone_model(global[3]);
  const Vec3 bone_forward =
      glm::normalize(Vec3(global * Vec4(0.0f, 1.0f, 0.0f, 0.0f)));
  const Mat4 host_inv = glm::inverse(object->computeWorldMatrix());
  const Vec3 target_model = Vec3(host_inv * Vec4(10.0f, 1.0f, 5.0f, 1.0f));
  const Vec3 desired = glm::normalize(target_model - bone_model);
  expect_true("aim toward world target in model",
              glm::dot(bone_forward, desired) > 0.9f);

  const Mat4 skin = skeleton->getBoneSkinMatrix(static_cast<size_t>(head));
  const Mat4 expected_skin =
      global * skeleton->getBoneInverseBind(static_cast<size_t>(head));
  expect_true("palette matches model global*ib", mat4_near(skin, expected_skin));
  // Object sits at world x=10; model Global / palette must not carry that +10.
  expect_true("global model x small", std::fabs(global[3].x) < 2.0f);
}

void test_look_at_updates_joint_palette_consumer() {
  using namespace Blunder;
  ObjectDB::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  Skeleton* skeleton = object->ensureSkeleton();
  skeleton->addBone("Root", -1);
  const int head = skeleton->addBone("Head", 0);
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 1.0f, 0.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->setBoneInverseBind(static_cast<size_t>(head), Mat4(1.0f));
  skeleton->resetPoseToRest();

  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0, 1};
  skin_data.influences.push_back({});
  skin_data.influences.push_back({});

  SkeletonLookAtModifier* look_at = object->addSkeletonLookAtModifier();
  look_at->setBoneName("Head");
  look_at->setTarget(Vec3(0.0f, 1.0f, 2.0f));

  AnimationPlayer* player = object->ensureAnimationPlayer();
  player->injectClipData("c", make_translate_clip("idle", "Head", Vec3(0.0f, 1.0f, 0.0f)));
  player->setClipGuid("idle", "c");
  player->play("idle");
  player->sampleOntoSkeleton(*skeleton);

  eastl::vector<Mat4> joints;
  buildGpuBonePalette(*skeleton, skin_data, joints);
  expect_true("joint count", joints.size() == 2);
  expect_true("joint matches pipeline skin",
              mat4_near(joints[1],
                        skeleton->getBoneSkinMatrix(static_cast<size_t>(head))));
}

void test_active_tree_blocks_player_slots() {
  using namespace Blunder;
  ObjectDB::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  Skeleton* skeleton = object->ensureSkeleton();
  skeleton->addBone("Root", -1);
  const int head = skeleton->addBone("Head", 0);
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  AnimationPlayer* player = object->ensureAnimationPlayer();
  player->injectClipData("slot",
                         make_translate_clip("slot", "Head", Vec3(9.0f, 0.0f, 0.0f)));
  player->injectClipData("tree",
                         make_translate_clip("tree", "Head", Vec3(1.0f, 0.0f, 0.0f)));
  player->setClipGuid("slot", "slot");
  player->setClipGuid("tree", "tree");
  player->setSlot(0, "slot");
  player->setBlendWeight(0.0f);
  player->play("slot");

  AnimationTree* tree = object->ensureAnimationTree();
  tree->setActive(true);
  expect_true("tree blocks player", player->isTreeBlockingSampling());
  tree->setSampleClipName("tree");
  tree->sampleBoundSkeleton();

  const float x =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).translation.x;
  expect_true("tree clip won not slot", float_near(x, 1.0f));
  expect_true("tree left palette valid", skeleton->hasValidPoseBuffers());

  // Player advance must not overwrite while tree active.
  player->advance(0.016f);
  const float x_after =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).translation.x;
  expect_true("player advance skipped", float_near(x_after, 1.0f));
}

}  // namespace

int main() {
  test_single_clip_local_global_palette();
  test_dual_slot_pose_applied_after_palette();
  test_look_at_world_target_and_palette();
  test_look_at_updates_joint_palette_consumer();
  test_active_tree_blocks_player_slots();

  if (g_failures > 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("animation_pipeline_test OK\n");
  return 0;
}
