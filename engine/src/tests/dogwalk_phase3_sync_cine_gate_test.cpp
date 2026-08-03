#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/cine_segment_service.h"
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

void bindClip(Blunder::AnimationPlayer& player, const char* clip_name,
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

struct MultiPlayerRig {
  Blunder::Object* character{nullptr};
  Blunder::Object* partner{nullptr};
  Blunder::AnimationPlayer* character_player{nullptr};
  Blunder::AnimationPlayer* partner_player{nullptr};
  Blunder::Skeleton* character_skeleton{nullptr};
  Blunder::Skeleton* partner_skeleton{nullptr};
  Blunder::SyncGroupId group{Blunder::k_invalid_sync_group_id};

  void setup() {
    using namespace Blunder;
    ObjectDB::clear();
    animationSyncGroupService().clearAll();
    cineSegmentService().resetForTests();

    const ObjectId character_id = ObjectDB::create();
    character = ObjectDB::get(character_id);
    const ObjectId partner_id = ObjectDB::create();
    partner = ObjectDB::get(partner_id);

    character_skeleton = character->ensureSkeleton();
    partner_skeleton = partner->ensureSkeleton();
    character_player = character->ensureAnimationPlayer();
    partner_player = partner->ensureAnimationPlayer();

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

    bindClip(*character_player, "CINE-character-attach",
             "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f,
             Vec3(0.0f, 4.0f, 0.0f));
    bindClip(*character_player, "idle",
             "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 2.0f,
             Vec3(0.0f, 0.0f, 0.0f));
    bindClip(*character_player, "walk",
             "cccccccc-cccc-cccc-cccc-cccccccccccc", 2.0f,
             Vec3(0.0f, 2.0f, 0.0f));

    bindClip(*partner_player, "CINE-prop-attach",
             "dddddddd-dddd-dddd-dddd-dddddddddddd", 2.0f,
             Vec3(0.0f, 3.0f, 0.0f));
    bindClip(*partner_player, "idle",
             "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee", 2.0f,
             Vec3(0.0f, 0.0f, 0.0f));

    group = animationSyncGroupService().create();
    expect_true("join character", animationSyncGroupService().join(group, character_player));
    expect_true("join partner", animationSyncGroupService().join(group, partner_player));
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
    cineSegmentService().resetForTests();
    character = nullptr;
    partner = nullptr;
    character_player = nullptr;
    partner_player = nullptr;
    character_skeleton = nullptr;
    partner_skeleton = nullptr;
    group = k_invalid_sync_group_id;
  }
};

/// Engineering gate: multi-Player Sync Group Fire with heterogeneous CINE clips.
void test_multi_player_sync_fire_aligned_start() {
  MultiPlayerRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr && rig.partner != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "CINE-character-attach"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "CINE-prop-attach"});
  expect_true("fire heterogeneous clips",
              animationSyncGroupService().fire(rig.group, instructions));

  expect_true("character playing", rig.character_player->isPlaying());
  expect_true("partner playing", rig.partner_player->isPlaying());
  expect_true("character clip",
              rig.character_player->getCurrentClipName() == "CINE-character-attach");
  expect_true("partner clip",
              rig.partner_player->getCurrentClipName() == "CINE-prop-attach");
  expect_true("character position zero",
              float_near(rig.character_player->getPlaybackPosition(), 0.0f));
  expect_true("partner position zero",
              float_near(rig.partner_player->getPlaybackPosition(), 0.0f));
  expect_true("not crossfading character", !rig.character_player->isCrossfading());
  expect_true("not crossfading partner", !rig.partner_player->isCrossfading());

  tickObjectAnimationPlayFrame(rig.character, 0.5f, /*play_paused=*/false);
  tickObjectAnimationPlayFrame(rig.partner, 0.5f, /*play_paused=*/false);

  const Vec3 character_tip = skinLegTip(*rig.character_skeleton);
  const Vec3 partner_tip = skinLegTip(*rig.partner_skeleton);
  expect_true("character deforms at half clip",
              vec3_near(character_tip, Vec3(0.0f, 1.0f, 0.0f), 1e-3f));
  expect_true("partner deforms at half clip",
              vec3_near(partner_tip, Vec3(0.0f, 0.75f, 0.0f), 1e-3f));
  expect_true("aligned playback positions",
              float_near(rig.character_player->getPlaybackPosition(),
                         rig.partner_player->getPlaybackPosition(), 1e-3f));

  rig.teardown();
}

/// Engineering gate: Fire hard-cuts mid dual-slot blend on both members.
void test_multi_player_sync_fire_hard_cut_from_blend() {
  MultiPlayerRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  rig.character_player->setSlot(0, "idle");
  rig.character_player->setSlot(1, "walk");
  rig.character_player->setBlendWeight(0.6f);
  rig.partner_player->setSlot(0, "idle");
  rig.partner_player->setBlendWeight(0.0f);
  expect_true("play character idle", rig.character_player->play("idle"));
  expect_true("play partner idle", rig.partner_player->play("idle"));
  tickObjectAnimationPlayFrame(rig.character, 0.4f, /*play_paused=*/false);
  tickObjectAnimationPlayFrame(rig.partner, 0.3f, /*play_paused=*/false);

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "CINE-character-attach"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "CINE-prop-attach"});
  expect_true("fire hard cut", animationSyncGroupService().fire(rig.group, instructions));

  expect_true("character cine clip",
              rig.character_player->getCurrentClipName() == "CINE-character-attach");
  expect_true("partner cine clip",
              rig.partner_player->getCurrentClipName() == "CINE-prop-attach");
  expect_true("character blend cleared",
              float_near(rig.character_player->getBlendWeight(), 0.0f));
  expect_true("partner blend cleared",
              float_near(rig.partner_player->getBlendWeight(), 0.0f));
  expect_true("character position reset",
              float_near(rig.character_player->getPlaybackPosition(), 0.0f));
  expect_true("partner position reset",
              float_near(rig.partner_player->getPlaybackPosition(), 0.0f));

  rig.teardown();
}

/// Engineering gate: CINE Enter/End marks and optional input suppression.
void test_cine_enter_end_marks() {
  using namespace Blunder;

  cineSegmentService().resetForTests();
  expect_true("not in cine initially", !cineSegmentService().isInCine());

  expect_true("enter with suppress", cineSegmentService().enter(true));
  expect_true("in cine after enter", cineSegmentService().isInCine());
  expect_true("input suppressed", cineSegmentService().isGameplayInputSuppressed());

  expect_true("end clears", cineSegmentService().end());
  expect_true("not in cine after end", !cineSegmentService().isInCine());
  expect_true("input restored", !cineSegmentService().isGameplayInputSuppressed());
  expect_true("end idempotent fails", !cineSegmentService().end());

  cineSegmentService().resetForTests();
}

/// Engineering gate: clip finished does not alone End the CINE segment.
void test_clip_finished_does_not_auto_end_cine() {
  MultiPlayerRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  bindClip(*rig.character_player, "CINE-short",
           "ffffffff-ffff-ffff-ffff-ffffffffffff", 0.2f, Vec3(0.0f, 1.0f, 0.0f));

  expect_true("enter cine", cineSegmentService().enter(true));
  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "CINE-short"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "CINE-prop-attach"});
  expect_true("fire", animationSyncGroupService().fire(rig.group, instructions));

  tickObjectAnimationPlayFrame(rig.character, 0.5f, /*play_paused=*/false);
  tickObjectAnimationPlayFrame(rig.partner, 0.5f, /*play_paused=*/false);

  expect_true("character clip finished", !rig.character_player->isPlaying());
  expect_true("still in cine after clip finished", cineSegmentService().isInCine());
  expect_true("still suppressed", cineSegmentService().isGameplayInputSuppressed());

  expect_true("explicit end required", cineSegmentService().end());
  expect_true("cleared after end", !cineSegmentService().isInCine());

  rig.teardown();
}

/// Engineering gate: integrated Sync Fire + tick + CINE cycle on multi-Object rig.
void test_integrated_sync_fire_cine_cycle() {
  MultiPlayerRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  expect_true("enter cine", cineSegmentService().enter(true));

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "CINE-character-attach"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "CINE-prop-attach"});
  expect_true("fire sync group", animationSyncGroupService().fire(rig.group, instructions));

  tickObjectAnimationPlayFrame(rig.character, 0.25f, /*play_paused=*/false);
  tickObjectAnimationPlayFrame(rig.partner, 0.25f, /*play_paused=*/false);
  expect_true("still in cine during playback", cineSegmentService().isInCine());

  expect_true("end cine", cineSegmentService().end());
  expect_true("cleared", !cineSegmentService().isInCine());

  tickObjectAnimationPlayFrame(rig.character, 0.25f, /*play_paused=*/false);
  tickObjectAnimationPlayFrame(rig.partner, 0.25f, /*play_paused=*/false);
  expect_true("playback continues after end",
              rig.character_player->isPlaying() && rig.partner_player->isPlaying());
  expect_true("positions stay aligned",
              float_near(rig.character_player->getPlaybackPosition(),
                         rig.partner_player->getPlaybackPosition(), 1e-3f));

  rig.teardown();
}

}  // namespace

int main() {
  test_multi_player_sync_fire_aligned_start();
  test_multi_player_sync_fire_hard_cut_from_blend();
  test_cine_enter_end_marks();
  test_clip_finished_does_not_auto_end_cine();
  test_integrated_sync_fire_cine_cycle();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase3_sync_cine_gate_test OK\n");
  return 0;
}
