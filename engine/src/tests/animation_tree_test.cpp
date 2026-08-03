#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/reflection/class_db.h"

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

}  // namespace

int main() {
  test_object_hosts_animation_tree_with_player_and_skeleton();
  test_tree_resolves_clip_guid_via_player_map();
  test_tree_resolves_clip_data_via_player_map();
  test_object_tree_resolves_through_hosted_player();
  test_classdb_animation_tree_registration();
  test_active_tree_blocks_player_play_and_two_slot_bone_writes();
  test_inactive_tree_restores_player_two_slot_sampling();
  test_object_binding_blocks_player_while_tree_active();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
