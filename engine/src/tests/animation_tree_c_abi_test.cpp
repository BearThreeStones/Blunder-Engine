#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "EASTL/string.h"

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

void bind_clip(Blunder::AnimationPlayer& player, const char* clip_name,
               const char* guid, float x_translation) {
  eastl::string guid_str(guid);
  Blunder::AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", Blunder::AnimationInterpolation::Constant,
      {{0.0f, Blunder::Vec3(x_translation, 0.0f, 0.0f)},
       {1.0f, Blunder::Vec3(x_translation, 0.0f, 0.0f)}}));
  player.setClipGuid(clip_name, guid_str);
  player.injectClipData(guid_str, clip);
}

void setup_tree_object(Blunder::ObjectId id, Blunder::Object* object) {
  object->ensureSkeleton()->addBone("Hips", -1);
  Blunder::AnimationPlayer* player = object->ensureAnimationPlayer();
  Blunder::AnimationTree* tree = object->ensureAnimationTree();
  bind_clip(*player, "idle", "11111111-1111-1111-1111-111111111111", 0.0f);
  bind_clip(*player, "walk", "22222222-2222-2222-2222-222222222222", 4.0f);
  bind_clip(*player, "trip", "33333333-3333-3333-3333-333333333333", 8.0f);
  tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
  tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
  tree->setStateBlendSpace("Locomotion", "Locomotion");
  tree->addBlendSpace2DPoint("Locomotion2D", "idle", 0.0f, 0.0f);
  tree->addBlendSpace2DPoint("Locomotion2D", "walk", 1.0f, 0.0f);
  tree->addBlendSpace2DPoint("Locomotion2D", "trip", 0.0f, 1.0f);
  tree->setStateBlendSpace2D("Move2D", "Locomotion2D");
}

}  // namespace

int main() {
  using namespace Blunder;

  ObjectDB::clear();
  ClassDB::initialize();
  animationSyncGroupService().clearAll();

  expect_true("abi version >= 11", blunder_engine_abi_version() >= 11);
  expect_true("abi version matches header",
              blunder_engine_abi_version() == BLUNDER_ENGINE_C_ABI_VERSION);

  BlunderNativeAbi abi{};
  blunder_native_abi_fill_from_process(&abi);
  expect_true("abi tree set active", abi.animation_tree_set_active != nullptr);
  expect_true("abi tree travel", abi.animation_tree_travel != nullptr);
  expect_true("abi tree request oneshot",
              abi.animation_tree_request_one_shot != nullptr);
  expect_true("abi tree set add2 weight",
              abi.animation_tree_set_add2_weight != nullptr);
  expect_true("abi tree set blend 2d",
              abi.animation_tree_set_blend_space_2d_param != nullptr);
  expect_true("abi tree set asset guid",
              abi.animation_tree_set_asset_guid != nullptr);

  const ObjectId native_id = ObjectDB::create();
  const BlunderObjectId id = static_cast<BlunderObjectId>(native_id);
  Object* object = ObjectDB::get(native_id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    ClassDB::shutdown();
    ObjectDB::clear();
    return 1;
  }

  setup_tree_object(native_id, object);

  expect_true("set active",
              blunder_animation_tree_set_active(id, 1) == BLUNDER_ENGINE_OK);
  int active = 0;
  expect_true("get active",
              blunder_animation_tree_get_active(id, &active) == BLUNDER_ENGINE_OK);
  expect_true("active true", active == 1);

  expect_true("travel",
              blunder_animation_tree_travel(id, "Locomotion") ==
                  BLUNDER_ENGINE_OK);
  expect_true("start",
              blunder_animation_tree_start(id, "Locomotion") == BLUNDER_ENGINE_OK);

  expect_true("set blend scalar",
              blunder_animation_tree_set_blend_space_scalar(id, "Locomotion",
                                                            0.5f) ==
                  BLUNDER_ENGINE_OK);
  float scalar = 0.0f;
  expect_true("get blend scalar",
              blunder_animation_tree_get_blend_space_scalar(id, "Locomotion",
                                                            &scalar) ==
                  BLUNDER_ENGINE_OK);
  expect_true("scalar value", float_near(scalar, 0.5f));

  expect_true("set blend 2d",
              blunder_animation_tree_set_blend_space_2d_param(
                  id, "Locomotion2D", 0.25f, 0.75f) == BLUNDER_ENGINE_OK);
  float blend_x = 0.0f;
  float blend_y = 0.0f;
  expect_true("get blend 2d",
              blunder_animation_tree_get_blend_space_2d_param(
                  id, "Locomotion2D", &blend_x, &blend_y) == BLUNDER_ENGINE_OK);
  expect_true("blend 2d x", float_near(blend_x, 0.25f));
  expect_true("blend 2d y", float_near(blend_y, 0.75f));

  expect_true("set tree param bool",
              blunder_animation_tree_set_tree_param_bool(id, "want_walk", 1) ==
                  BLUNDER_ENGINE_OK);
  int want_walk = 0;
  expect_true("get tree param bool",
              blunder_animation_tree_get_tree_param_bool(id, "want_walk",
                                                         &want_walk) ==
                  BLUNDER_ENGINE_OK);
  expect_true("want_walk true", want_walk == 1);
  expect_true("set tree param float",
              blunder_animation_tree_set_tree_param_float(id, "speed", 0.8f) ==
                  BLUNDER_ENGINE_OK);
  float speed = 0.0f;
  expect_true("get tree param float",
              blunder_animation_tree_get_tree_param_float(id, "speed", &speed) ==
                  BLUNDER_ENGINE_OK);
  expect_true("speed value", float_near(speed, 0.8f));

  expect_true("set asset guid",
              blunder_animation_tree_set_asset_guid(
                  id, "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa") ==
                  BLUNDER_ENGINE_OK);
  char guid_buf[64] = {};
  expect_true("get asset guid",
              blunder_animation_tree_get_asset_guid(
                  id, guid_buf, static_cast<int>(sizeof(guid_buf))) ==
                  BLUNDER_ENGINE_OK);
  expect_true("asset guid value",
              std::strcmp(guid_buf, "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa") ==
                  0);

  expect_true("set add2 weight",
              blunder_animation_tree_set_add2_weight(id, 0.25f) ==
                  BLUNDER_ENGINE_OK);
  float add2 = 0.0f;
  expect_true("get add2 weight",
              blunder_animation_tree_get_add2_weight(id, &add2) ==
                  BLUNDER_ENGINE_OK);
  expect_true("add2 value", float_near(add2, 0.25f));

  expect_true("request oneshot",
              blunder_animation_tree_request_one_shot(id, "trip") ==
                  BLUNDER_ENGINE_OK);
  expect_true("oneshot active", object->getAnimationTree()->isOneShotActive());

  const SyncGroupId group = animationSyncGroupService().create();
  expect_true("sync group join",
              animationSyncGroupService().join(group,
                                               object->getAnimationPlayer()));
  SyncGroupFireInstruction instruction;
  instruction.player = object->getAnimationPlayer();
  instruction.clip_name = "trip";
  expect_true("sync fire routes to oneshot",
              animationSyncGroupService().fire(group, {instruction}));
  expect_true("tree still active after fire",
              object->getAnimationTree()->isActive());
  expect_true("oneshot still active after fire",
              object->getAnimationTree()->isOneShotActive());

  animationSyncGroupService().clearAll();
  ClassDB::shutdown();
  ObjectDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("animation_tree_c_abi_test: all passed\n");
  return 0;
}
