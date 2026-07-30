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
  // Tip vertex fully weighted to Leg (bone 1).
  skin_data.influences[0].joint_indices = glm::ivec4(1, 0, 0, 0);
  skin_data.influences[0].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

  eastl::vector<MeshVertex> bind(1);
  bind[0].position = Vec3(0.0f, 0.0f, 0.0f);
  bind[0].normal = Vec3(0.0f, 0.0f, 1.0f);

  eastl::vector<MeshVertex> skinned;
  applyCpuSkinning(skeleton, skin_data, bind, skinned);
  return skinned[0].position;
}

/// Engineering gate: idle↔walk hard cut + skinned deformation (test-rig topology).
void test_idle_walk_hard_cut_deforms() {
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
  const int leg = skeleton->addBone("Leg", hips);
  skeleton->setBoneInverseBind(static_cast<size_t>(hips), Mat4(1.0f));
  skeleton->setBoneInverseBind(static_cast<size_t>(leg), Mat4(1.0f));
  skeleton->resetPoseToRest();

  const eastl::string idle_guid = "100c1644-5e5f-40d2-b4fb-55485629c15e";
  const eastl::string walk_guid = "205b3534-8c30-4484-bee6-9a6ad6b5dd35";

  AnimationClipData idle;
  idle.name = "idle";
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Leg", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.name = "walk";
  walk.duration = 1.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Leg", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 2.0f, 0.0f)}}));

  player->setClipGuid("idle", idle_guid);
  player->setClipGuid("walk", walk_guid);
  player->injectClipData(idle_guid, idle);
  player->injectClipData(walk_guid, walk);

  expect_true("play idle", player->play("idle"));
  expect_true("current idle", player->getCurrentClipName() == "idle");
  tickObjectAnimationPlayFrame(object, 0.5f, /*play_paused=*/false);
  const Vec3 idle_tip = skinLegTip(*skeleton);
  expect_true("idle pose holds rest tip",
              vec3_near(idle_tip, Vec3(0.0f, 0.0f, 0.0f)));

  // Hard cut: Play walk without Stop — position resets to clip start then advances.
  expect_true("hard cut walk", player->play("walk"));
  expect_true("current walk", player->getCurrentClipName() == "walk");
  expect_true("hard cut resets position",
              float_near(player->getPlaybackPosition(), 0.0f));
  tickObjectAnimationPlayFrame(object, 0.5f, /*play_paused=*/false);
  const Vec3 walk_tip = skinLegTip(*skeleton);
  expect_true("walk deforms skinned tip",
              vec3_near(walk_tip, Vec3(0.0f, 1.0f, 0.0f)));
  expect_true("walk tip differs from idle",
              !vec3_near(walk_tip, idle_tip));

  expect_true("hard cut back to idle", player->play("idle"));
  expect_true("current idle again", player->getCurrentClipName() == "idle");
  tickObjectAnimationPlayFrame(object, 0.25f, /*play_paused=*/false);
  const Vec3 idle_again = skinLegTip(*skeleton);
  expect_true("idle restores rest tip after cut",
              vec3_near(idle_again, Vec3(0.0f, 0.0f, 0.0f)));

  ObjectDB::destroy(id);
  ObjectDB::clear();
}

}  // namespace

int main() {
  test_idle_walk_hard_cut_deforms();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_test_rig_play_acceptance_test OK\n");
  return 0;
}
