#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/animation_tree.h"
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

struct TreePlayRig {
  Blunder::Object* character{nullptr};
  Blunder::AnimationPlayer* player{nullptr};
  Blunder::AnimationTree* tree{nullptr};
  Blunder::Skeleton* skeleton{nullptr};

  void setup() {
    using namespace Blunder;
    ObjectDB::clear();

    const ObjectId id = ObjectDB::create();
    character = ObjectDB::get(id);
    skeleton = character->ensureSkeleton();
    player = character->ensureAnimationPlayer();
    tree = character->ensureAnimationTree();

    const int hips = skeleton->addBone("Hips", -1);
    const int leg = skeleton->addBone("Leg", hips);
    skeleton->setBoneInverseBind(static_cast<size_t>(hips), Mat4(1.0f));
    skeleton->setBoneInverseBind(static_cast<size_t>(leg), Mat4(1.0f));
    skeleton->resetPoseToRest();
    skeleton->setBoneRestLocal(leg, BoneTransform{});

    bindPoseClip(*player, "idle",
                 "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f,
                 Vec3(0.0f, 0.0f, 0.0f));
    bindPoseClip(*player, "walk",
                 "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 2.0f,
                 Vec3(0.0f, 2.0f, 0.0f));
    bindPoseClip(*player, "turn_add",
                 "cccccccc-cccc-cccc-cccc-cccccccccccc", 1.0f,
                 Vec3(0.0f, 3.0f, 0.0f));
    bindPoseClip(*player, "trip",
                 "dddddddd-dddd-dddd-dddd-dddddddddddd", 0.4f,
                 Vec3(0.0f, 10.0f, 0.0f));

    tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
    tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
    tree->setStateBlendSpace("Locomotion", "Locomotion");
    tree->setAdd2ClipName("turn_add");
    expect_true("activate tree", tree->setActive(true));
    expect_true("travel locomotion", tree->travel("Locomotion"));
  }

  void tickPlay(float dt) {
    tickObjectAnimationPlayFrame(character, dt, /*play_paused=*/false);
  }

  void teardown() {
    using namespace Blunder;
    if (character != nullptr) {
      ObjectDB::destroy(character->getId());
    }
    ObjectDB::clear();
    character = nullptr;
    player = nullptr;
    tree = nullptr;
    skeleton = nullptr;
  }
};

/// Mini Play acceptance (automated): BlendSpace scalar changes perceptible pose under Play tick.
void test_play_blend_space_scalar_motion() {
  TreePlayRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.tree->setBlendSpaceScalar("Locomotion", 0.0f);
  rig.tickPlay(0.001f);
  const Blunder::Vec3 idle_tip = skinLegTip(*rig.skeleton);

  rig.tree->setBlendSpaceScalar("Locomotion", 1.0f);
  rig.tickPlay(0.001f);
  const Blunder::Vec3 walk_tip = skinLegTip(*rig.skeleton);
  expect_true("blend scalar changes skinned pose",
              vec3_near(idle_tip, Blunder::Vec3(0.0f, 0.0f, 0.0f), 1e-3f));
  expect_true("walk scalar deforms",
              vec3_near(walk_tip, Blunder::Vec3(0.0f, 2.0f, 0.0f), 1e-3f));
  expect_true("idle vs walk differ", !vec3_near(idle_tip, walk_tip, 1e-3f));

  rig.teardown();
}

/// Mini Play acceptance (automated): visible Add2 overlay on locomotion base.
void test_play_add2_turn_overlay() {
  TreePlayRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.tree->setBlendSpaceScalar("Locomotion", 0.5f);
  rig.tree->setAdd2Weight(0.0f);
  rig.tickPlay(0.001f);
  const Blunder::Vec3 base_tip = skinLegTip(*rig.skeleton);

  rig.tree->setAdd2Weight(0.5f);
  rig.tickPlay(0.001f);
  const Blunder::Vec3 add_tip = skinLegTip(*rig.skeleton);
  expect_true("add2 changes skinned pose",
              !vec3_near(base_tip, add_tip, 1e-3f));
  expect_true("add2 overlay midpoint",
              vec3_near(add_tip, Blunder::Vec3(0.0f, 2.5f, 0.0f), 1e-3f));

  rig.teardown();
}

/// Mini Play acceptance (automated): OneShot returns to BlendSpace base after Play ticks.
void test_play_oneshot_return_to_base() {
  TreePlayRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.tree->setBlendSpaceScalar("Locomotion", 0.5f);
  rig.tickPlay(0.001f);
  const Blunder::Vec3 base_tip = skinLegTip(*rig.skeleton);

  expect_true("request trip", rig.tree->requestOneShot("trip"));
  rig.tickPlay(0.1f);
  expect_true("trip pose differs from base",
              !vec3_near(skinLegTip(*rig.skeleton), base_tip, 1e-3f));

  rig.tickPlay(0.5f);
  expect_true("one-shot finished", !rig.tree->isOneShotActive());
  expect_true("returned to blend base",
              vec3_near(skinLegTip(*rig.skeleton), base_tip, 1e-3f));

  rig.teardown();
}

/// Mini Play acceptance (automated): dominant base clock advances under Play; Add2 ignored.
void test_play_dominant_base_clock_step_sync() {
  TreePlayRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  rig.tree->setBlendSpaceScalar("Locomotion", 1.0f);
  rig.tree->setAdd2Weight(1.0f);
  rig.tree->setAdd2Time(0.8f);
  rig.tickPlay(0.35f);

  expect_true("dominant walk length",
              float_near(rig.player->getClipLength(), 2.0f));
  expect_true("base clock advanced",
              float_near(rig.player->getPlaybackPosition(), 0.35f, 1e-3f));
  expect_true("add2 time does not own playback clock",
              float_near(rig.player->getPlaybackPosition(), 0.35f, 1e-3f));

  rig.teardown();
}

/// Mini Play acceptance (automated): Sync Fire on active tree member uses OneShot during Play tick.
void test_play_sync_fire_oneshot_on_active_tree() {
  using namespace Blunder;

  TreePlayRig rig;
  rig.setup();
  expect_true("rig created", rig.character != nullptr);
  if (rig.character == nullptr) {
    return;
  }

  bindPoseClip(*rig.player, "SYNC-trip",
               "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee", 0.5f,
               Blunder::Vec3(0.0f, 9.0f, 0.0f));

  rig.tree->setBlendSpaceScalar("Locomotion", 0.25f);
  rig.tickPlay(0.1f);

  animationSyncGroupService().clearAll();
  const SyncGroupId group = animationSyncGroupService().create();
  expect_true("join player", animationSyncGroupService().join(group, rig.player));

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{rig.player, "SYNC-trip"});
  expect_true("fire one-shot via sync", animationSyncGroupService().fire(group, instructions));

  expect_true("tree still active", rig.tree->isActive());
  expect_true("one-shot active", rig.tree->isOneShotActive());
  expect_true("not hard-cut playing", !rig.player->isPlaying());

  rig.tickPlay(0.6f);
  expect_true("one-shot finished after play ticks", !rig.tree->isOneShotActive());
  expect_true("locomotion state preserved",
              rig.tree->getCurrentStateName() == "Locomotion");

  animationSyncGroupService().clearAll();
  rig.teardown();
}

}  // namespace

int main() {
  test_play_blend_space_scalar_motion();
  test_play_add2_turn_overlay();
  test_play_oneshot_return_to_base();
  test_play_dominant_base_clock_step_sync();
  test_play_sync_fire_oneshot_on_active_tree();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase4_mini_play_acceptance_test OK\n");
  return 0;
}
