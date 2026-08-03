#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/class_db.h"

#include <glm/gtc/quaternion.hpp>
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

bool float_near(float a, float b, float eps = 1e-5f) {
  return std::fabs(a - b) < eps;
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = 1e-4f) {
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

Blunder::AnimationTrack makeRotationTrack(
    const char* bone, Blunder::AnimationInterpolation interpolation,
    std::initializer_list<std::pair<float, Blunder::Quat>> keys) {
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = Blunder::AnimationChannel::Rotation;
  track.interpolation = interpolation;
  for (const auto& key : keys) {
    Blunder::AnimationKeyframe frame;
    frame.time = key.first;
    frame.value = {key.second.x, key.second.y, key.second.z, key.second.w};
    track.keys.push_back(frame);
  }
  return track;
}

bool quat_near(const Blunder::Quat& a, const Blunder::Quat& b,
               float eps = 1e-4f) {
  const float dot = std::fabs(glm::dot(a, b));
  return float_near(dot, 1.0f, eps);
}

Blunder::Skeleton makeSingleBoneSkeleton(const char* bone_name) {
  Blunder::Skeleton skeleton;
  skeleton.addBone(bone_name, -1);
  return skeleton;
}

Blunder::AnimationClipData make_test_clip(const char* name, float duration) {
  Blunder::AnimationClipData clip;
  clip.name = name;
  clip.duration = duration;
  return clip;
}

void test_object_hosts_animation_tree_with_player_and_skeleton() {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  expect_true("no tree initially", !object->hasAnimationTree());
  expect_true("get tree null", object->getAnimationTree() == nullptr);

  AnimationTree* tree = object->ensureAnimationTree();
  expect_true("ensure returns tree", tree != nullptr);
  expect_true("has tree", object->hasAnimationTree());
  expect_true("get matches ensure", object->getAnimationTree() == tree);

  AnimationTree* again = object->ensureAnimationTree();
  expect_true("ensure idempotent", again == tree);

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  expect_true("co-located skeleton", skeleton != nullptr);
  expect_true("co-located player", player != nullptr);
  expect_true("tree still same", object->getAnimationTree() == tree);

  object->clearAnimationTree();
  expect_true("cleared", !object->hasAnimationTree());
  expect_true("get null after clear", object->getAnimationTree() == nullptr);

  ObjectDB::clear();
}

void test_tree_resolves_clip_guid_via_player_map() {
  using namespace Blunder;

  AnimationPlayer player;
  AnimationTree tree;
  tree.bindAnimationPlayer(&player);

  const eastl::string walk_guid = "33333333-3333-3333-3333-333333333333";
  player.setClipGuid("LOOP-chocomel-walk", walk_guid);

  eastl::string resolved_guid;
  expect_true("resolve known clip name",
              tree.resolveClipGuid("LOOP-chocomel-walk", resolved_guid));
  expect_true("guid matches player map", resolved_guid == walk_guid);
  expect_true("unknown name fails",
              !tree.resolveClipGuid("missing-clip", resolved_guid));
}

void test_tree_resolves_clip_data_via_player_map() {
  using namespace Blunder;

  AnimationPlayer player;
  AnimationTree tree;
  tree.bindAnimationPlayer(&player);

  const eastl::string walk_guid = "33333333-3333-3333-3333-333333333333";
  player.setClipGuid("LOOP-chocomel-walk", walk_guid);
  player.injectClipData(walk_guid, make_test_clip("LOOP-chocomel-walk", 1.25f));

  AnimationClipData clip;
  expect_true("resolve clip for name",
              tree.resolveClipForName("LOOP-chocomel-walk", clip));
  expect_true("clip duration", clip.duration == 1.25f);
  expect_true("missing clip fails",
              !tree.resolveClipForName("missing-clip", clip));
}

void test_object_tree_resolves_through_hosted_player() {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  expect_true("tree bound to object player", tree != nullptr);

  const eastl::string walk_guid = "44444444-4444-4444-4444-444444444444";
  player->setClipGuid("LOOP-chocomel-walk", walk_guid);
  player->injectClipData(walk_guid, make_test_clip("LOOP-chocomel-walk", 2.0f));

  eastl::string resolved_guid;
  expect_true("object tree resolves guid",
              tree->resolveClipGuid("LOOP-chocomel-walk", resolved_guid));
  expect_true("guid from hosted player", resolved_guid == walk_guid);

  AnimationClipData clip;
  expect_true("object tree resolves clip",
              tree->resolveClipForName("LOOP-chocomel-walk", clip));
  expect_true("clip duration from hosted player", clip.duration == 2.0f);

  ObjectDB::clear();
}

void test_classdb_animation_tree_registration() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("AnimationTree registered", ClassDB::hasClass("AnimationTree"));

  AnimationPlayer player;
  AnimationTree tree;
  tree.bindAnimationPlayer(&player);

  const eastl::string walk_guid = "55555555-5555-5555-5555-555555555555";
  player.setClipGuid("LOOP-chocomel-walk", walk_guid);

  eastl::string resolved_guid;
  tree.resolveClipGuid("LOOP-chocomel-walk", resolved_guid);

  Variant has_player;
  expect_true("has_animation_player property",
              ClassDB::getProperty(&tree, "AnimationTree", "has_animation_player",
                                   has_player));
  expect_true("has_animation_player true", has_player.asBool());

  ClassDB::shutdown();
}

void test_active_tree_blocks_player_play_and_two_slot_bone_writes() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  const eastl::string tree_guid = "33333333-3333-3333-3333-333333333333";

  AnimationClipData idle;
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.duration = 1.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(10.0f, 0.0f, 0.0f)}, {1.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  AnimationClipData tree_pose;
  tree_pose.duration = 1.0f;
  tree_pose.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(7.0f, 0.0f, 0.0f)}, {1.0f, Vec3(7.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.setClipGuid("tree_pose", tree_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);
  player.injectClipData(tree_guid, tree_pose);

  expect_true("tree inactive initially", !tree.isActive());
  expect_true("set tree sample clip", tree.setSampleClipName("tree_pose"));
  expect_true("activate tree", tree.setActive(true));
  expect_true("tree active", tree.isActive());
  expect_true("tree pose after activate",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(7.0f, 0.0f, 0.0f)));

  player.setSlot(0, "idle");
  player.setSlot(1, "walk");
  player.setBlendWeight(0.5f);
  expect_true("player play while tree active", player.play("idle"));
  expect_true("player still blocked by tree",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(7.0f, 0.0f, 0.0f)));

  player.advance(0.25f);
  expect_true("advance does not apply player two-slot blend",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(7.0f, 0.0f, 0.0f)));
}

void test_inactive_tree_restores_player_two_slot_sampling() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  const eastl::string tree_guid = "33333333-3333-3333-3333-333333333333";

  AnimationClipData idle;
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.duration = 1.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(10.0f, 0.0f, 0.0f)}, {1.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  AnimationClipData tree_pose;
  tree_pose.duration = 1.0f;
  tree_pose.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(7.0f, 0.0f, 0.0f)}, {1.0f, Vec3(7.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.setClipGuid("tree_pose", tree_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);
  player.injectClipData(tree_guid, tree_pose);

  player.setSlot(0, "idle");
  player.setSlot(1, "walk");
  player.setBlendWeight(0.5f);
  expect_true("player play while tree inactive", player.play("idle"));
  expect_true("player dual-slot pose",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(5.0f, 0.0f, 0.0f)));

  tree.setSampleClipName("tree_pose");
  expect_true("activate tree", tree.setActive(true));
  expect_true("tree overrides player pose",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(7.0f, 0.0f, 0.0f)));

  expect_true("deactivate tree", tree.setActive(false));
  expect_true("player path restored after deactivate",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(5.0f, 0.0f, 0.0f)));
}

void test_base_then_add2_bind_rest_additive_not_lerp_dual_track() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  skeleton.setBoneRestLocal(0, BoneTransform{});

  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string base_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string add2_guid = "22222222-2222-2222-2222-222222222222";

  AnimationClipData base_clip;
  base_clip.duration = 1.0f;
  base_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(4.0f, 0.0f, 0.0f)}, {1.0f, Vec3(4.0f, 0.0f, 0.0f)}}));

  AnimationClipData add2_clip;
  add2_clip.duration = 1.0f;
  add2_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(2.0f, 0.0f, 0.0f)}, {1.0f, Vec3(2.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("locomotion", base_guid);
  player.setClipGuid("turn_add", add2_guid);
  player.injectClipData(base_guid, base_clip);
  player.injectClipData(add2_guid, add2_clip);

  expect_true("set base clip", tree.setSampleClipName("locomotion"));
  expect_true("set add2 clip", tree.setAdd2ClipName("turn_add"));
  tree.setAdd2Weight(0.5f);
  expect_true("activate tree", tree.setActive(true));

  const Vec3 additive_expected(5.0f, 0.0f, 0.0f);
  expect_true("base then bind/rest additive",
              vec3_near(skeleton.getBonePoseLocal(0).translation, additive_expected));

  const Vec3 lerp_dual_track(3.0f, 0.0f, 0.0f);
  expect_true("not phase2 lerp dual-track",
              !vec3_near(skeleton.getBonePoseLocal(0).translation, lerp_dual_track));

  tree.setAdd2Weight(0.0f);
  tree.sampleBoundSkeleton();
  expect_true("add2 weight zero skips additive",
              vec3_near(skeleton.getBonePoseLocal(0).translation, Vec3(4.0f, 0.0f, 0.0f)));
}

void test_object_binding_blocks_player_while_tree_active() {
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
  AnimationTree* tree = object->ensureAnimationTree();

  const eastl::string idle_guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  const eastl::string walk_guid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  const eastl::string tree_guid = "cccccccc-cccc-cccc-cccc-cccccccccccc";

  AnimationClipData idle;
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.duration = 1.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(10.0f, 0.0f, 0.0f)}, {1.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  AnimationClipData tree_pose;
  tree_pose.duration = 1.0f;
  tree_pose.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(6.0f, 0.0f, 0.0f)}, {1.0f, Vec3(6.0f, 0.0f, 0.0f)}}));

  skeleton->addBone("Hips", -1);
  player->setClipGuid("idle", idle_guid);
  player->setClipGuid("walk", walk_guid);
  player->setClipGuid("tree_pose", tree_guid);
  player->injectClipData(idle_guid, idle);
  player->injectClipData(walk_guid, walk);
  player->injectClipData(tree_guid, tree_pose);

  tree->setSampleClipName("tree_pose");
  expect_true("object tree activate", tree->setActive(true));
  expect_true("object tree pose",
              vec3_near(skeleton->getBonePoseLocal(0).translation,
                        Vec3(6.0f, 0.0f, 0.0f)));

  player->setSlot(0, "idle");
  player->setSlot(1, "walk");
  player->setBlendWeight(0.5f);
  expect_true("object player play blocked", player->play("idle"));
  expect_true("object skeleton still tree pose",
              vec3_near(skeleton->getBonePoseLocal(0).translation,
                        Vec3(6.0f, 0.0f, 0.0f)));

  expect_true("object tree deactivate", tree->setActive(false));
  expect_true("object player restored",
              vec3_near(skeleton->getBonePoseLocal(0).translation,
                        Vec3(5.0f, 0.0f, 0.0f)));

  ObjectDB::clear();
}

void test_blend_space1d_neighbor_lerp_feeds_base_pose() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";

  AnimationClipData idle;
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.duration = 1.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(10.0f, 0.0f, 0.0f)}, {1.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);

  expect_true("add idle point", tree.addBlendSpacePoint("Locomotion", "idle", 0.0f));
  expect_true("add walk point", tree.addBlendSpacePoint("Locomotion", "walk", 1.0f));
  expect_true("set base blend space", tree.setBaseBlendSpaceNode("Locomotion"));
  tree.setBlendSpaceScalar("Locomotion", 0.5f);
  expect_true("activate tree", tree.setActive(true));

  expect_true("neighbor lerp midpoint",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(5.0f, 0.0f, 0.0f)));
}

void test_blend_space1d_clamps_endpoints() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";

  AnimationClipData idle;
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.duration = 1.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(10.0f, 0.0f, 0.0f)}, {1.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);

  tree.addBlendSpacePoint("Locomotion", "idle", 0.0f);
  tree.addBlendSpacePoint("Locomotion", "walk", 1.0f);
  tree.setBaseBlendSpaceNode("Locomotion");
  expect_true("activate tree", tree.setActive(true));

  tree.setBlendSpaceScalar("Locomotion", -1.0f);
  tree.sampleBoundSkeleton();
  expect_true("clamp below min",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(0.0f, 0.0f, 0.0f)));

  tree.setBlendSpaceScalar("Locomotion", 2.0f);
  tree.sampleBoundSkeleton();
  expect_true("clamp above max",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(10.0f, 0.0f, 0.0f)));
}

void test_blend_space1d_scalar_by_node_logical_name() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string slow_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string fast_guid = "22222222-2222-2222-2222-222222222222";

  AnimationClipData slow;
  slow.duration = 1.0f;
  slow.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData fast;
  fast.duration = 1.0f;
  fast.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(8.0f, 0.0f, 0.0f)}, {1.0f, Vec3(8.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("slow", slow_guid);
  player.setClipGuid("fast", fast_guid);
  player.injectClipData(slow_guid, slow);
  player.injectClipData(fast_guid, fast);

  tree.addBlendSpacePoint("Locomotion", "slow", 0.0f);
  tree.addBlendSpacePoint("Locomotion", "fast", 1.0f);
  tree.addBlendSpacePoint("UpperBody", "slow", 0.0f);
  tree.addBlendSpacePoint("UpperBody", "fast", 1.0f);
  tree.setBaseBlendSpaceNode("Locomotion");
  tree.setBlendSpaceScalar("Locomotion", 1.0f);
  tree.setBlendSpaceScalar("UpperBody", 0.0f);
  expect_true("locomotion scalar stored", tree.getBlendSpaceScalar("Locomotion") == 1.0f);
  expect_true("upper body scalar stored", tree.getBlendSpaceScalar("UpperBody") == 0.0f);
  expect_true("activate tree", tree.setActive(true));

  expect_true("base uses locomotion scalar only",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(8.0f, 0.0f, 0.0f)));
}

void test_blend_space1d_rotation_uses_slerp() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string left_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string right_guid = "22222222-2222-2222-2222-222222222222";

  const Quat rot_a = glm::angleAxis(glm::radians(0.0f), Vec3(0.0f, 1.0f, 0.0f));
  const Quat rot_b = glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 1.0f, 0.0f));
  const Quat expected = glm::slerp(rot_a, rot_b, 0.5f);

  AnimationClipData clip_a;
  clip_a.duration = 1.0f;
  clip_a.tracks.push_back(makeRotationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, rot_a}, {1.0f, rot_a}}));

  AnimationClipData clip_b;
  clip_b.duration = 1.0f;
  clip_b.tracks.push_back(makeRotationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, rot_b}, {1.0f, rot_b}}));

  player.setClipGuid("left", left_guid);
  player.setClipGuid("right", right_guid);
  player.injectClipData(left_guid, clip_a);
  player.injectClipData(right_guid, clip_b);

  tree.addBlendSpacePoint("Locomotion", "left", 0.0f);
  tree.addBlendSpacePoint("Locomotion", "right", 1.0f);
  tree.setBaseBlendSpaceNode("Locomotion");
  tree.setBlendSpaceScalar("Locomotion", 0.5f);
  expect_true("activate tree", tree.setActive(true));

  expect_true("rotation slerp midpoint",
              quat_near(skeleton.getBonePoseLocal(0).rotation, expected));
}

void test_blend_space1d_base_then_add2_stacks() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  skeleton.setBoneRestLocal(0, BoneTransform{});

  AnimationPlayer player;
  AnimationTree tree;
  player.bindSamplingSkeleton(&skeleton);
  tree.bindAnimationPlayer(&player);
  tree.bindSamplingSkeleton(&skeleton);

  const eastl::string slow_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string fast_guid = "22222222-2222-2222-2222-222222222222";
  const eastl::string add2_guid = "33333333-3333-3333-3333-333333333333";

  AnimationClipData slow;
  slow.duration = 1.0f;
  slow.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));

  AnimationClipData fast;
  fast.duration = 1.0f;
  fast.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(8.0f, 0.0f, 0.0f)}, {1.0f, Vec3(8.0f, 0.0f, 0.0f)}}));

  AnimationClipData add2_clip;
  add2_clip.duration = 1.0f;
  add2_clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(2.0f, 0.0f, 0.0f)}, {1.0f, Vec3(2.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("slow", slow_guid);
  player.setClipGuid("fast", fast_guid);
  player.setClipGuid("turn_add", add2_guid);
  player.injectClipData(slow_guid, slow);
  player.injectClipData(fast_guid, fast);
  player.injectClipData(add2_guid, add2_clip);

  tree.addBlendSpacePoint("Locomotion", "slow", 0.0f);
  tree.addBlendSpacePoint("Locomotion", "fast", 1.0f);
  tree.setBaseBlendSpaceNode("Locomotion");
  tree.setBlendSpaceScalar("Locomotion", 0.5f);
  expect_true("set add2 clip", tree.setAdd2ClipName("turn_add"));
  tree.setAdd2Weight(0.5f);
  expect_true("activate tree", tree.setActive(true));

  const Vec3 expected(5.0f, 0.0f, 0.0f);
  expect_true("blend space base then add2 additive",
              vec3_near(skeleton.getBonePoseLocal(0).translation, expected));
}

}  // namespace

int main() {
  test_object_hosts_animation_tree_with_player_and_skeleton();
  test_tree_resolves_clip_guid_via_player_map();
  test_tree_resolves_clip_data_via_player_map();
  test_object_tree_resolves_through_hosted_player();
  test_classdb_animation_tree_registration();
  test_active_tree_blocks_player_play_and_two_slot_bone_writes();
  test_inactive_tree_restores_player_two_slot_sampling();
  test_base_then_add2_bind_rest_additive_not_lerp_dual_track();
  test_object_binding_blocks_player_while_tree_active();
  test_blend_space1d_neighbor_lerp_feeds_base_pose();
  test_blend_space1d_clamps_endpoints();
  test_blend_space1d_scalar_by_node_logical_name();
  test_blend_space1d_rotation_uses_slerp();
  test_blend_space1d_base_then_add2_stacks();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
