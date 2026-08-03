#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_preview_controller.h"
#include "runtime/function/scene/cpu_skinning.h"
#include "runtime/function/script/animation_frame.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/mesh_skin_data.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b, float eps = 1e-4f) {
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

Blunder::Vec3 skinLegTip(const Blunder::Skeleton& skeleton) {
  using namespace Blunder;
  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0, 1};
  skin_data.influences.push_back({});
  skin_data.influences[0].joint_indices = glm::ivec4(1, 0, 0, 0);
  skin_data.influences[0].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

  eastl::vector<MeshVertex> bind(1);
  bind[0].position = Vec3(0.0f, 0.0f, 0.0f);
  bind[0].normal = Vec3(0.0f, 0.0f, 1.0f);

  eastl::vector<MeshVertex> skinned;
  applyCpuSkinning(skeleton, skin_data, bind, skinned);
  return skinned[0].position;
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

void bindMotionClip(Blunder::AnimationPlayer& player, const char* clip_name,
                    const char* guid, float duration,
                    const Blunder::Vec3& end_translation) {
  using namespace Blunder;
  eastl::string guid_str(guid);
  AnimationClipData clip;
  clip.name = clip_name;
  clip.duration = duration;
  clip.tracks.push_back(makeTranslationTrack(
      "Leg", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {duration, end_translation}}));
  player.setClipGuid(clip_name, guid_str);
  player.injectClipData(guid_str, clip);
}

struct Phase4TreeRig {
  Blunder::Object* character{nullptr};
  Blunder::Object* partner{nullptr};
  Blunder::AnimationPlayer* character_player{nullptr};
  Blunder::AnimationPlayer* partner_player{nullptr};
  Blunder::AnimationTree* character_tree{nullptr};
  Blunder::Skeleton* character_skeleton{nullptr};
  Blunder::Skeleton* partner_skeleton{nullptr};
  Blunder::SyncGroupId group{Blunder::k_invalid_sync_group_id};

  void setup() {
    using namespace Blunder;
    ObjectDB::clear();
    animationSyncGroupService().clearAll();

    const ObjectId character_id = ObjectDB::create();
    character = ObjectDB::get(character_id);
    const ObjectId partner_id = ObjectDB::create();
    partner = ObjectDB::get(partner_id);

    character_skeleton = character->ensureSkeleton();
    partner_skeleton = partner->ensureSkeleton();
    character_player = character->ensureAnimationPlayer();
    partner_player = partner->ensureAnimationPlayer();
    character_tree = character->ensureAnimationTree();

    const int character_hips = character_skeleton->addBone("Hips", -1);
    const int character_leg = character_skeleton->addBone("Leg", character_hips);
    character_skeleton->setBoneInverseBind(static_cast<size_t>(character_hips),
                                           Mat4(1.0f));
    character_skeleton->setBoneInverseBind(static_cast<size_t>(character_leg),
                                           Mat4(1.0f));
    character_skeleton->resetPoseToRest();

    const int partner_hips = partner_skeleton->addBone("Hips", -1);
    const int partner_leg = partner_skeleton->addBone("Leg", partner_hips);
    partner_skeleton->setBoneInverseBind(static_cast<size_t>(partner_hips),
                                         Mat4(1.0f));
    partner_skeleton->setBoneInverseBind(static_cast<size_t>(partner_leg),
                                         Mat4(1.0f));
    partner_skeleton->resetPoseToRest();

    bindPoseClip(*character_player, "idle",
                 "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f,
                 Vec3(0.0f, 0.0f, 0.0f));
    bindPoseClip(*character_player, "walk",
                 "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 2.0f,
                 Vec3(0.0f, 2.0f, 0.0f));
    bindPoseClip(*character_player, "turn_add",
                 "cccccccc-cccc-cccc-cccc-cccccccccccc", 1.0f,
                 Vec3(0.0f, 2.0f, 0.0f));
    bindPoseClip(*character_player, "trip",
                 "dddddddd-dddd-dddd-dddd-dddddddddddd", 0.5f,
                 Vec3(0.0f, 12.0f, 0.0f));
    bindPoseClip(*character_player, "SYNC-attach",
                 "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee", 1.0f,
                 Vec3(0.0f, 8.0f, 0.0f));

    bindPoseClip(*partner_player, "SYNC-prop",
                 "ffffffff-ffff-ffff-ffff-ffffffffffff", 1.0f,
                 Vec3(0.0f, 3.0f, 0.0f));

    character_tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
    character_tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
    character_tree->setStateBlendSpace("Locomotion", "Locomotion");
    character_tree->setAdd2ClipName("turn_add");

    group = animationSyncGroupService().create();
    expect_true("join character", animationSyncGroupService().join(group, character_player));
    expect_true("join partner", animationSyncGroupService().join(group, partner_player));
  }

  void activateLocomotion(float scalar = 0.5f) {
    using namespace Blunder;
    expect_true("activate tree", character_tree->setActive(true));
    expect_true("travel locomotion", character_tree->travel("Locomotion"));
    character_tree->setBlendSpaceScalar("Locomotion", scalar);
    character_tree->sampleBoundSkeleton();
  }

  void teardown() {
    using namespace Blunder;
    if (character != nullptr) {
      ObjectDB::destroy(character->getId());
    }
    if (partner != nullptr) {
      ObjectDB::destroy(partner->getId());
    }
    ObjectDB::clear();
    animationSyncGroupService().clearAll();
    character = nullptr;
    partner = nullptr;
    character_player = nullptr;
    partner_player = nullptr;
    character_tree = nullptr;
    character_skeleton = nullptr;
    partner_skeleton = nullptr;
    group = k_invalid_sync_group_id;
  }
};

/// Engineering gate: Travel + BlendSpace1D scalar changes skinned pose.
void test_travel_blend_space1d_deforms() {
  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.activateLocomotion(0.0f);
  expect_true("idle blend tip",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 0.0f, 0.0f)));

  rig.character_tree->setBlendSpaceScalar("Locomotion", 1.0f);
  rig.character_tree->sampleBoundSkeleton();
  expect_true("walk blend tip",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 2.0f, 0.0f),
                        1e-3f));

  rig.character_tree->setBlendSpaceScalar("Locomotion", 0.5f);
  rig.character_tree->sampleBoundSkeleton();
  expect_true("mid blend tip",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 1.0f, 0.0f),
                        1e-3f));

  rig.teardown();
}

/// Engineering gate: Add2 overlay stacks on BlendSpace base (bind/rest additive).
void test_add2_overlay_on_blend_space_base() {
  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.character_skeleton->setBoneRestLocal(1, Blunder::BoneTransform{});
  rig.activateLocomotion(0.5f);
  rig.character_tree->setAdd2Weight(0.5f);
  rig.character_tree->sampleBoundSkeleton();

  // Base midpoint y=1.0 + 0.5 * add2 y=2.0 = 2.0 on Leg bone.
  expect_true("add2 visible on blend base",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 2.0f, 0.0f),
                        1e-3f));

  rig.teardown();
}

/// Engineering gate: OneShot inserts then returns to BlendSpace base.
void test_oneshot_returns_to_blend_space_base() {
  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.activateLocomotion(0.5f);
  expect_true("request trip", rig.character_tree->requestOneShot("trip"));
  expect_true("trip pose",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 12.0f, 0.0f),
                        1e-3f));

  rig.character_tree->advance(0.6f);
  expect_true("one-shot finished", !rig.character_tree->isOneShotActive());
  expect_true("returned to blend base",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 1.0f, 0.0f),
                        1e-3f));
  expect_true("locomotion state preserved",
              rig.character_tree->getCurrentStateName() == "Locomotion");

  rig.teardown();
}

/// Engineering gate: Sync Fire on active-tree member applies OneShot (not hard-cut Play).
void test_sync_fire_applies_oneshot_on_active_tree() {
  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  rig.activateLocomotion(0.25f);

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "SYNC-attach"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "SYNC-prop"});
  expect_true("mixed fire", animationSyncGroupService().fire(rig.group, instructions));

  expect_true("tree stays active", rig.character_tree->isActive());
  expect_true("character one-shot active", rig.character_tree->isOneShotActive());
  expect_true("character not hard-cut playing", !rig.character_player->isPlaying());
  expect_true("attach pose",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 8.0f, 0.0f),
                        1e-3f));
  expect_true("partner hard-cut playing", rig.partner_player->isPlaying());
  expect_true("partner sync clip",
              rig.partner_player->getCurrentClipName() == "SYNC-prop");

  rig.teardown();
}

/// Engineering gate: active tree blocks Player two-slot bone writes.
void test_active_tree_exclusive_sampling() {
  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.activateLocomotion(1.0f);
  const Blunder::Vec3 tree_tip = skinLegTip(*rig.character_skeleton);

  rig.character_player->setSlot(0, "idle");
  rig.character_player->setSlot(1, "walk");
  rig.character_player->setBlendWeight(0.0f);
  expect_true("player play blocked", rig.character_player->play("idle"));
  tickObjectAnimationPlayFrame(rig.character, 0.5f, /*play_paused=*/false);
  expect_true("skeleton still tree pose",
              vec3_near(skinLegTip(*rig.character_skeleton), tree_tip, 1e-3f));

  expect_true("deactivate tree", rig.character_tree->setActive(false));
  tickObjectAnimationPlayFrame(rig.character, 0.001f, /*play_paused=*/false);
  expect_true("player path restored",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 0.0f, 0.0f),
                        1e-3f));

  rig.teardown();
}

/// Engineering gate: PoseApplied fires; playback position follows base dominant clock.
void test_pose_applied_and_dominant_base_clock() {
  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  int pose_count = 0;
  rig.character_player->addPoseAppliedListener(
      [](Blunder::AnimationPlayer&, void* userdata) {
        ++*static_cast<int*>(userdata);
      },
      &pose_count);

  rig.activateLocomotion(0.75f);
  pose_count = 0;
  rig.character_tree->advance(0.4f);
  expect_true("pose applied after advance", pose_count >= 1);
  expect_true("dominant walk length at high scalar",
              float_near(rig.character_player->getClipLength(), 2.0f));
  expect_true("playback follows base sample time",
              float_near(rig.character_player->getPlaybackPosition(), 0.4f));

  rig.character_tree->setAdd2Weight(1.0f);
  rig.character_tree->setAdd2Time(0.9f);
  rig.character_tree->sampleBoundSkeleton();
  expect_true("add2 does not drive playback clock",
              float_near(rig.character_player->getPlaybackPosition(), 0.4f));

  rig.teardown();
}

/// Engineering gate: global TimeScale on AnimationPlayer advances active tree.
void test_time_scale_advances_active_tree() {
  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.character_player->setTimeScale(2.0f);
  bindMotionClip(*rig.character_player, "walk",
                 "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 2.0f,
                 Blunder::Vec3(0.0f, 4.0f, 0.0f));
  rig.activateLocomotion(1.0f);
  tickObjectAnimationPlayFrame(rig.character, 0.25f, /*play_paused=*/false);

  expect_true("time scale doubles tree advance",
              float_near(rig.character_player->getPlaybackPosition(), 0.5f, 1e-3f));
  expect_true("walk pose at half clip",
              vec3_near(skinLegTip(*rig.character_skeleton), Blunder::Vec3(0.0f, 1.0f, 0.0f),
                        1e-3f));

  rig.teardown();
}

/// Engineering gate: Edit preview scrubs tree drives without Behaviour Tick.
void test_edit_preview_tree_scrub_without_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object",
                                 [](void*, float) { ++g_tick_calls; });

  Phase4TreeRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  int peer = 0;
  rig.character->addBehaviour("Object");
  rig.character->setBehaviourScriptPeer(rig.character->getBehaviourIdAt(0), &peer);

  AnimationPreviewController controller;
  controller.bindObject(rig.character, "walk");
  expect_true("activate tree", controller.setTreeActive(true));
  expect_true("travel", controller.travel("Locomotion"));
  controller.setBlendSpaceScalar("Locomotion", 0.5f);
  controller.setAdd2Weight(0.25f);
  controller.setTimeScale(2.0f);
  expect_true("play", controller.play());
  controller.tick(0.25f);

  expect_true("no behaviour tick during edit scrub", g_tick_calls == 0);
  expect_true("blend scalar scrubbed",
              float_near(controller.blendSpaceScalar("Locomotion"), 0.5f));
  expect_true("timeScale advances preview",
              float_near(controller.playbackPosition(), 0.5f));
  expect_true("tree still active", controller.isTreeActive());

  LifecycleDispatch::clear();
  rig.teardown();
}

}  // namespace

int main() {
  test_travel_blend_space1d_deforms();
  test_add2_overlay_on_blend_space_base();
  test_oneshot_returns_to_blend_space_base();
  test_sync_fire_applies_oneshot_on_active_tree();
  test_active_tree_exclusive_sampling();
  test_pose_applied_and_dominant_base_clock();
  test_time_scale_advances_active_tree();
  test_edit_preview_tree_scrub_without_behaviour_tick();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase4_tree_gate_test OK\n");
  return 0;
}
