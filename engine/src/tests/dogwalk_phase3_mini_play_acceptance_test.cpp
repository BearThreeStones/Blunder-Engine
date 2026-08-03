#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/cine_segment_service.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/function/scene/cpu_skinning.h"
#include "runtime/function/script/animation_frame.h"
#include "runtime/platform/input/gameplay_input.h"
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

struct PlayAcceptanceRig {
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
    gameplayInputState().reset();

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

    bindClip(*character_player, "SYNC-walk",
             "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f,
             Vec3(0.0f, 4.0f, 0.0f));
    bindClip(*partner_player, "SYNC-prop-wave",
             "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 2.0f,
             Vec3(0.0f, 2.0f, 0.0f));

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
    gameplayInputState().reset();
    character = nullptr;
    partner = nullptr;
    character_player = nullptr;
    partner_player = nullptr;
    character_skeleton = nullptr;
    partner_skeleton = nullptr;
    group = k_invalid_sync_group_id;
  }

  void tickPlay(float dt) {
    tickObjectAnimationPlayFrame(character, dt, /*play_paused=*/false);
    tickObjectAnimationPlayFrame(partner, dt, /*play_paused=*/false);
  }
};

/// Mini Play acceptance (automated): character + partner synchronized Sync Group start.
void test_play_sync_group_synchronized_start() {
  PlayAcceptanceRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "SYNC-walk"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "SYNC-prop-wave"});
  expect_true("fire sync group",
              animationSyncGroupService().fire(rig.group, instructions));

  rig.tickPlay(0.5f);

  expect_true("character playing", rig.character_player->isPlaying());
  expect_true("partner playing", rig.partner_player->isPlaying());
  expect_true("aligned positions",
              float_near(rig.character_player->getPlaybackPosition(),
                         rig.partner_player->getPlaybackPosition(), 1e-3f));

  const Vec3 character_tip = skinLegTip(*rig.character_skeleton);
  const Vec3 partner_tip = skinLegTip(*rig.partner_skeleton);
  expect_true("character deforms",
              vec3_near(character_tip, Vec3(0.0f, 1.0f, 0.0f), 1e-3f));
  expect_true("partner deforms",
              vec3_near(partner_tip, Vec3(0.0f, 0.5f, 0.0f), 1e-3f));
  expect_true("heterogeneous clips",
              rig.character_player->getCurrentClipName() == "SYNC-walk" &&
                  rig.partner_player->getCurrentClipName() == "SYNC-prop-wave");

  rig.teardown();
}

/// Mini Play acceptance (automated): CINE handoff suppresses then restores gameplay input.
void test_play_cine_handoff_restores_control() {
  PlayAcceptanceRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  GameplayInputKeys keys{};
  keys.player_host = true;
  keys.focused = true;
  keys.d = true;

  auto before = gameplayInputState().sample(keys);
  expect_true("move before cine", float_near(before.move_x, 1.0f, 1e-3f));

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "SYNC-walk"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "SYNC-prop-wave"});
  expect_true("fire", animationSyncGroupService().fire(rig.group, instructions));
  expect_true("enter cine", cineSegmentService().enter(true));

  auto during = gameplayInputState().sample(keys);
  expect_true("move suppressed in cine", float_near(during.move_x, 0.0f));
  rig.tickPlay(0.25f);
  expect_true("animation still advances in cine",
              float_near(rig.character_player->getPlaybackPosition(), 0.25f, 1e-3f));

  expect_true("end cine", cineSegmentService().end());
  auto after = gameplayInputState().sample(keys);
  expect_true("move restored after end", float_near(after.move_x, 1.0f, 1e-3f));
  expect_true("not in cine", !cineSegmentService().isInCine());

  rig.teardown();
}

/// Mini Play acceptance (automated): explicit End required — clip finish alone does not exit CINE.
void test_play_explicit_end_required() {
  PlayAcceptanceRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  using namespace Blunder;
  bindClip(*rig.character_player, "CINE-short",
           "cccccccc-cccc-cccc-cccc-cccccccccccc", 0.15f, Vec3(0.0f, 1.0f, 0.0f));

  expect_true("enter cine", cineSegmentService().enter(true));
  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(
      SyncGroupFireInstruction{rig.character_player, "CINE-short"});
  instructions.push_back(
      SyncGroupFireInstruction{rig.partner_player, "SYNC-prop-wave"});
  expect_true("fire", animationSyncGroupService().fire(rig.group, instructions));

  rig.tickPlay(0.5f);
  expect_true("character clip finished", !rig.character_player->isPlaying());
  expect_true("still in cine", cineSegmentService().isInCine());

  GameplayInputKeys keys{};
  keys.player_host = true;
  keys.focused = true;
  keys.w = true;
  auto suppressed = gameplayInputState().sample(keys);
  expect_true("input still suppressed", float_near(suppressed.move_y, 0.0f));

  expect_true("explicit end", cineSegmentService().end());
  auto restored = gameplayInputState().sample(keys);
  expect_true("input restored", float_near(restored.move_y, 1.0f, 1e-3f));

  rig.teardown();
}

}  // namespace

int main() {
  test_play_sync_group_synchronized_start();
  test_play_cine_handoff_restores_control();
  test_play_explicit_end_required();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase3_mini_play_acceptance_test OK\n");
  return 0;
}
