#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/scene/cpu_skinning.h"
#include "runtime/function/script/animation_frame.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/mesh_skin_data.h"

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

struct TestRig {
  Blunder::Object* object{nullptr};
  Blunder::Skeleton* skeleton{nullptr};
  Blunder::AnimationPlayer* player{nullptr};

  void setup(bool linear_walk = false) {
    using namespace Blunder;
    ObjectDB::clear();
    const ObjectId id = ObjectDB::create();
    object = ObjectDB::get(id);
    skeleton = object->ensureSkeleton();
    player = object->ensureAnimationPlayer();
    const int hips = skeleton->addBone("Hips", -1);
    const int leg = skeleton->addBone("Leg", hips);
    skeleton->setBoneInverseBind(static_cast<size_t>(hips), Mat4(1.0f));
    skeleton->setBoneInverseBind(static_cast<size_t>(leg), Mat4(1.0f));
    skeleton->resetPoseToRest();

    const eastl::string idle_guid = "100c1644-5e5f-40d2-b4fb-55485629c15e";
    const eastl::string walk_guid = "205b3534-8c30-4484-bee6-9a6ad6b5dd35";

    AnimationClipData idle;
    idle.name = "idle";
    idle.duration = 2.0f;
    idle.tracks.push_back(makeTranslationTrack(
        "Leg", AnimationInterpolation::Constant,
        {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {2.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

    AnimationClipData walk;
    walk.name = "walk";
    walk.duration = 2.0f;
    if (linear_walk) {
      walk.tracks.push_back(makeTranslationTrack(
          "Leg", AnimationInterpolation::Linear,
          {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {2.0f, Vec3(0.0f, 4.0f, 0.0f)}}));
    } else {
      walk.tracks.push_back(makeTranslationTrack(
          "Leg", AnimationInterpolation::Constant,
          {{0.0f, Vec3(0.0f, 2.0f, 0.0f)}, {2.0f, Vec3(0.0f, 2.0f, 0.0f)}}));
    }

    player->setClipGuid("idle", idle_guid);
    player->setClipGuid("walk", walk_guid);
    player->injectClipData(idle_guid, idle);
    player->injectClipData(walk_guid, walk);
  }

  void teardown() {
    if (object != nullptr) {
      Blunder::ObjectDB::destroy(object->getId());
    }
    Blunder::ObjectDB::clear();
    object = nullptr;
    skeleton = nullptr;
    player = nullptr;
  }
};

/// Engineering gate: two-slot idle+walk blend at mid weight deforms skinned mesh.
void test_two_slot_mid_blend_deforms() {
  TestRig rig;
  rig.setup();
  expect_true("rig created", rig.object != nullptr);
  if (rig.object == nullptr) {
    return;
  }

  rig.player->setSlot(0, "idle");
  rig.player->setSlot(1, "walk");
  rig.player->setBlendWeight(0.5f);
  expect_true("play idle", rig.player->play("idle"));
  tickObjectAnimationPlayFrame(rig.object, 0.001f, /*play_paused=*/false);

  const Blunder::Vec3 tip = skinLegTip(*rig.skeleton);
  expect_true("mid blend deforms tip halfway",
              vec3_near(tip, Blunder::Vec3(0.0f, 1.0f, 0.0f)));

  rig.teardown();
}

/// Engineering gate: Crossfade play ramps weight and blended pose on test-rig topology.
void test_crossfade_play_ramps_pose() {
  TestRig rig;
  rig.setup();
  expect_true("rig created", rig.object != nullptr);
  if (rig.object == nullptr) {
    return;
  }

  rig.player->setSlot(0, "idle");
  rig.player->setBlendWeight(0.0f);
  expect_true("play idle", rig.player->play("idle"));
  tickObjectAnimationPlayFrame(rig.object, 0.001f, /*play_paused=*/false);

  expect_true("crossfade to walk", rig.player->play("walk", 1.0f));
  expect_true("crossfading", rig.player->isCrossfading());
  expect_true("target slot1 walk", rig.player->getSlotClipName(1) == "walk");

  tickObjectAnimationPlayFrame(rig.object, 0.5f, /*play_paused=*/false);
  expect_true("weight halfway", float_near(rig.player->getBlendWeight(), 0.5f, 1e-3f));
  const Blunder::Vec3 mid_tip = skinLegTip(*rig.skeleton);
  expect_true("crossfade mid-ramp pose",
              vec3_near(mid_tip, Blunder::Vec3(0.0f, 1.0f, 0.0f), 1e-3f));

  tickObjectAnimationPlayFrame(rig.object, 0.5f, /*play_paused=*/false);
  expect_true("crossfade complete", !rig.player->isCrossfading());
  expect_true("weight full walk", float_near(rig.player->getBlendWeight(), 1.0f, 1e-3f));

  rig.teardown();
}

/// Engineering gate: TimeScale != 1 accelerates both slots (observed via dominant pose).
void test_time_scale_accelerates_dual_slots() {
  TestRig rig;
  rig.setup(/*linear_walk=*/true);
  expect_true("rig created", rig.object != nullptr);
  if (rig.object == nullptr) {
    return;
  }

  rig.player->setSlot(0, "idle");
  rig.player->setSlot(1, "walk");
  rig.player->setBlendWeight(0.5f);
  rig.player->setTimeScale(2.0f);
  expect_true("play dual slots", rig.player->play("idle"));

  tickObjectAnimationPlayFrame(rig.object, 0.5f, /*play_paused=*/false);
  // 0.5 real seconds * 2.0 time scale = 1.0s on both slots.
  // idle leg stays 0, walk leg at 2.0s clip → y=2.0 at t=1.0 → blend y=1.0.
  const Blunder::Vec3 tip = skinLegTip(*rig.skeleton);
  expect_true("time scale advances both slots",
              vec3_near(tip, Blunder::Vec3(0.0f, 1.0f, 0.0f), 1e-3f));

  rig.teardown();
}

/// Engineering gate: dominant-slot playback position tracks step sync clock.
void test_dominant_slot_step_sync() {
  TestRig rig;
  rig.setup();
  expect_true("rig created", rig.object != nullptr);
  if (rig.object == nullptr) {
    return;
  }

  rig.player->setSlot(0, "idle");
  rig.player->setSlot(1, "walk");
  rig.player->setBlendWeight(0.0f);
  expect_true("play idle", rig.player->play("idle"));
  tickObjectAnimationPlayFrame(rig.object, 0.6f, /*play_paused=*/false);
  expect_true("slot0 dominant position",
              float_near(rig.player->getPlaybackPosition(), 0.6f));

  rig.player->setSlot(1, "walk");
  rig.player->setBlendWeight(0.75f);
  expect_true("walk dominant at start",
              float_near(rig.player->getPlaybackPosition(), 0.0f));

  tickObjectAnimationPlayFrame(rig.object, 0.4f, /*play_paused=*/false);
  expect_true("dominant slot advances with ticks",
              float_near(rig.player->getPlaybackPosition(), 0.4f));

  rig.player->setBlendWeight(0.25f);
  expect_true("idle dominant resumes slot0 clock",
              float_near(rig.player->getPlaybackPosition(), 1.0f));

  rig.teardown();
}

}  // namespace

int main() {
  test_two_slot_mid_blend_deforms();
  test_crossfade_play_ramps_pose();
  test_time_scale_accelerates_dual_slots();
  test_dominant_slot_step_sync();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase2_blend_gate_test OK\n");
  return 0;
}
