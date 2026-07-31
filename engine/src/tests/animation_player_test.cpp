#include "runtime/core/object/animation_player.h"
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

void test_clip_name_guid_map() {
  using namespace Blunder;

  AnimationPlayer player;
  expect_true("empty map", player.getClipMapEntryCount() == 0);

  player.setClipGuid("walk", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  expect_true("one entry", player.getClipMapEntryCount() == 1);

  eastl::string guid;
  expect_true("get guid", player.getClipGuid("walk", guid));
  expect_true("guid value",
              guid == "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  expect_true("missing name", !player.getClipGuid("idle", guid));

  player.clearClipGuid("walk");
  expect_true("cleared", player.getClipMapEntryCount() == 0);

  player.setClipGuid("idle", "11111111-1111-1111-1111-111111111111");
  player.setClipGuid("walk", "22222222-2222-2222-2222-222222222222");
  expect_true("two entries", player.getClipMapEntryCount() == 2);
  player.clearAllClipGuids();
  expect_true("all cleared", player.getClipMapEntryCount() == 0);
}

void test_play_stop_loop_and_advance() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(walk_guid, make_test_clip("walk", 2.0f));

  expect_true("not playing initially", !player.isPlaying());
  expect_true("play succeeds", player.play("walk"));
  expect_true("playing", player.isPlaying());
  expect_true("current name", player.getCurrentClipName() == "walk");
  expect_true("length", float_near(player.getClipLength(), 2.0f));
  expect_true("position zero", float_near(player.getPlaybackPosition(), 0.0f));

  player.advance(0.5f);
  expect_true("advanced", float_near(player.getPlaybackPosition(), 0.5f));

  player.setLoop(true);
  expect_true("looping", player.isLooping());
  player.advance(1.8f);
  expect_true("wraps when looping",
              float_near(player.getPlaybackPosition(), 0.3f));
  expect_true("still playing when looping", player.isPlaying());

  player.setLoop(false);
  player.advance(10.0f);
  expect_true("clamped at end",
              float_near(player.getPlaybackPosition(), 2.0f));
  expect_true("stopped at end", !player.isPlaying());

  player.play("walk");
  player.stop();
  expect_true("stop clears playing", !player.isPlaying());
  expect_true("stop resets position",
              float_near(player.getPlaybackPosition(), 0.0f));
}

void test_hard_cut_between_clips() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 1.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 3.0f));

  expect_true("play idle", player.play("idle"));
  player.advance(0.7f);
  expect_true("idle advanced", float_near(player.getPlaybackPosition(), 0.7f));

  expect_true("hard cut to walk", player.play("walk"));
  expect_true("position reset", float_near(player.getPlaybackPosition(), 0.0f));
  expect_true("walk length", float_near(player.getClipLength(), 3.0f));
  expect_true("current walk", player.getCurrentClipName() == "walk");
  expect_true("still playing", player.isPlaying());
}

void test_unknown_play_name_no_crash() {
  using namespace Blunder;

  AnimationPlayer player;
  expect_true("unknown name fails", !player.play("missing"));
  expect_true("not playing", !player.isPlaying());
  expect_true("zero length", float_near(player.getClipLength(), 0.0f));
}

void test_object_hosts_animation_player() {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  expect_true("no player initially", !object->hasAnimationPlayer());
  expect_true("get null", object->getAnimationPlayer() == nullptr);

  AnimationPlayer* player = object->ensureAnimationPlayer();
  expect_true("ensure returns player", player != nullptr);
  expect_true("has player", object->hasAnimationPlayer());
  expect_true("get matches ensure", object->getAnimationPlayer() == player);

  AnimationPlayer* again = object->ensureAnimationPlayer();
  expect_true("ensure idempotent", again == player);

  object->clearAnimationPlayer();
  expect_true("cleared", !object->hasAnimationPlayer());
  expect_true("get null after clear", object->getAnimationPlayer() == nullptr);

  ObjectDB::clear();
}

void test_two_slot_assignment_and_blend_weight() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 1.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 2.0f));

  expect_true("default blend weight zero", float_near(player.getBlendWeight(), 0.0f));
  expect_true("slot0 empty initially", player.getSlotClipName(0).empty());
  expect_true("slot1 empty initially", player.getSlotClipName(1).empty());

  expect_true("set slot0 idle", player.setSlot(0, "idle"));
  expect_true("slot0 name", player.getSlotClipName(0) == "idle");
  expect_true("set slot1 walk", player.setSlot(1, "walk"));
  expect_true("slot1 name", player.getSlotClipName(1) == "walk");

  expect_true("unknown slot name fails", !player.setSlot(0, "missing"));
  expect_true("invalid slot index fails", !player.setSlot(2, "idle"));

  player.setBlendWeight(0.5f);
  expect_true("blend weight set", float_near(player.getBlendWeight(), 0.5f));
  player.setBlendWeight(1.5f);
  expect_true("blend weight clamped high", float_near(player.getBlendWeight(), 1.0f));
  player.setBlendWeight(-0.25f);
  expect_true("blend weight clamped low", float_near(player.getBlendWeight(), 0.0f));
}

void test_global_time_scale_default_and_set() {
  using namespace Blunder;

  AnimationPlayer player;
  expect_true("default time scale", float_near(player.getTimeScale(), 1.0f));

  player.setTimeScale(0.5f);
  expect_true("time scale set", float_near(player.getTimeScale(), 0.5f));
  player.setTimeScale(2.0f);
  expect_true("time scale updated", float_near(player.getTimeScale(), 2.0f));
}

void test_dual_slot_weighted_blend_on_skeleton() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  player.bindSamplingSkeleton(&skeleton);

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
  player.setSlot(0, "idle");
  player.setSlot(1, "walk");
  player.setBlendWeight(0.5f);

  expect_true("play starts sampling", player.play("idle"));
  player.advance(0.0f);
  expect_true("weighted dual-slot pose",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(5.0f, 0.0f, 0.0f)));
}

void test_single_slot_fallback() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  player.bindSamplingSkeleton(&skeleton);

  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  AnimationClipData idle;
  idle.duration = 1.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(3.0f, 0.0f, 0.0f)}, {1.0f, Vec3(3.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.injectClipData(idle_guid, idle);
  player.setSlot(0, "idle");
  player.setBlendWeight(1.0f);

  expect_true("play slot0 only", player.play("idle"));
  player.advance(0.0f);
  expect_true("slot0 only pose",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(3.0f, 0.0f, 0.0f)));
}

void test_pose_applied_after_dual_slot_sample() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  player.bindSamplingSkeleton(&skeleton);

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
      {{0.0f, Vec3(4.0f, 0.0f, 0.0f)}, {1.0f, Vec3(4.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);
  player.setSlot(0, "idle");
  player.setSlot(1, "walk");
  player.setBlendWeight(0.25f);

  int pose_count = 0;
  player.addPoseAppliedListener(
      [](AnimationPlayer&, void* userdata) {
        ++*static_cast<int*>(userdata);
      },
      &pose_count);

  expect_true("play", player.play("idle"));
  player.advance(0.0f);
  expect_true("pose applied once", pose_count == 1);
}

void test_play_with_zero_fade_is_hard_cut() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 1.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 3.0f));

  expect_true("play idle", player.play("idle"));
  player.advance(0.7f);
  expect_true("idle advanced", float_near(player.getPlaybackPosition(), 0.7f));

  expect_true("zero fade hard cut", player.play("walk", 0.0f));
  expect_true("position reset", float_near(player.getPlaybackPosition(), 0.0f));
  expect_true("walk length", float_near(player.getClipLength(), 3.0f));
  expect_true("current walk", player.getCurrentClipName() == "walk");
  expect_true("not crossfading", !player.isCrossfading());
}

void test_crossfade_ramps_blend_weight_over_time() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 2.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 2.0f));

  expect_true("play idle", player.play("idle"));
  player.setSlot(0, "idle");
  player.setBlendWeight(0.0f);

  expect_true("crossfade to walk", player.play("walk", 1.0f));
  expect_true("crossfading", player.isCrossfading());
  expect_true("weight starts at zero", float_near(player.getBlendWeight(), 0.0f));
  expect_true("target slot1 walk", player.getSlotClipName(1) == "walk");

  player.advance(0.5f);
  expect_true("weight halfway", float_near(player.getBlendWeight(), 0.5f, 1e-4f));
  expect_true("still crossfading", player.isCrossfading());

  player.advance(0.5f);
  expect_true("weight complete", float_near(player.getBlendWeight(), 1.0f, 1e-4f));
  expect_true("crossfade done", !player.isCrossfading());
}

void test_crossfade_blends_pose_mid_ramp() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  player.bindSamplingSkeleton(&skeleton);

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
      {{0.0f, Vec3(8.0f, 0.0f, 0.0f)}, {1.0f, Vec3(8.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);
  player.setSlot(0, "idle");
  player.setBlendWeight(0.0f);

  expect_true("crossfade play", player.play("walk", 1.0f));
  player.advance(0.5f);
  expect_true("mid-ramp pose",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(4.0f, 0.0f, 0.0f), 1e-3f));
}

void test_time_scale_scales_slot_advance() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  player.setClipGuid("idle", idle_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 10.0f));
  player.setSlot(0, "idle");
  player.setTimeScale(2.0f);

  expect_true("play slot", player.play("idle"));
  player.advance(0.5f);
  expect_true("scaled slot advance",
              float_near(player.getPlaybackPosition(), 1.0f));
}

void test_time_scale_advances_both_slots_via_blend_pose() {
  using namespace Blunder;

  Skeleton skeleton = makeSingleBoneSkeleton("Hips");
  AnimationPlayer player;
  player.bindSamplingSkeleton(&skeleton);

  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";

  AnimationClipData idle;
  idle.duration = 10.0f;
  idle.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {10.0f, Vec3(10.0f, 0.0f, 0.0f)}}));

  AnimationClipData walk;
  walk.duration = 10.0f;
  walk.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {10.0f, Vec3(20.0f, 0.0f, 0.0f)}}));

  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, idle);
  player.injectClipData(walk_guid, walk);
  player.setSlot(0, "idle");
  player.setSlot(1, "walk");
  player.setBlendWeight(0.5f);
  player.setTimeScale(2.0f);

  expect_true("play dual slots", player.play("idle"));
  player.advance(0.5f);
  // 0.5 real seconds * 2.0 time scale = 1.0s on both slots → blend (1 + 2) / 2.
  expect_true("both slots scaled via blend pose",
              vec3_near(skeleton.getBonePoseLocal(0).translation,
                        Vec3(1.5f, 0.0f, 0.0f), 1e-3f));
}

void test_time_scale_scales_phase1_advance() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(walk_guid, make_test_clip("walk", 10.0f));
  player.setTimeScale(0.5f);

  expect_true("play", player.play("walk"));
  player.advance(1.0f);
  expect_true("scaled phase1 advance",
              float_near(player.getPlaybackPosition(), 0.5f));
}

void test_time_scale_scales_crossfade_ramp() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 2.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 2.0f));

  player.setSlot(0, "idle");
  player.setBlendWeight(0.0f);
  player.setTimeScale(2.0f);
  expect_true("crossfade", player.play("walk", 1.0f));

  player.advance(0.25f);
  expect_true("scaled crossfade halfway",
              float_near(player.getBlendWeight(), 0.5f, 1e-4f));
}

void test_playback_position_dominant_slot_by_weight() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 10.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 10.0f));

  player.setSlot(0, "idle");
  expect_true("play idle", player.play("idle"));
  player.advance(0.6f);
  expect_true("slot0 advanced", float_near(player.getPlaybackPosition(), 0.6f));

  expect_true("assign slot1", player.setSlot(1, "walk"));
  player.setBlendWeight(0.75f);
  expect_true("dominant slot1 at zero",
              float_near(player.getPlaybackPosition(), 0.0f));

  player.setBlendWeight(0.25f);
  expect_true("dominant slot0", float_near(player.getPlaybackPosition(), 0.6f));

  player.setBlendWeight(0.5f);
  expect_true("tie prefers slot0", float_near(player.getPlaybackPosition(), 0.6f));
}

void test_playback_position_crossfade_uses_target_slot() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 10.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 10.0f));

  player.setSlot(0, "idle");
  player.setBlendWeight(0.0f);
  expect_true("crossfade to walk", player.play("walk", 1.0f));
  expect_true("crossfading", player.isCrossfading());
  expect_true("target slot1 even at weight zero",
              float_near(player.getPlaybackPosition(), 0.0f));

  player.advance(0.4f);
  expect_true("still crossfading mid-ramp", player.isCrossfading());
  expect_true("reports target slot position",
              float_near(player.getPlaybackPosition(), 0.4f));
}

void test_crossfade_from_phase1_single_clip() {
  using namespace Blunder;

  AnimationPlayer player;
  const eastl::string idle_guid = "11111111-1111-1111-1111-111111111111";
  const eastl::string walk_guid = "22222222-2222-2222-2222-222222222222";
  player.setClipGuid("idle", idle_guid);
  player.setClipGuid("walk", walk_guid);
  player.injectClipData(idle_guid, make_test_clip("idle", 1.0f));
  player.injectClipData(walk_guid, make_test_clip("walk", 2.0f));

  expect_true("phase1 play idle", player.play("idle"));
  player.advance(0.3f);
  expect_true("no slots yet", player.getSlotClipName(0).empty());

  expect_true("crossfade from phase1", player.play("walk", 0.5f));
  expect_true("source seeded slot0", player.getSlotClipName(0) == "idle");
  expect_true("target on slot1", player.getSlotClipName(1) == "walk");
  expect_true("crossfading", player.isCrossfading());

  player.advance(0.5f);
  expect_true("ramp complete", float_near(player.getBlendWeight(), 1.0f, 1e-4f));
}

void test_classdb_animation_player_registration() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("AnimationPlayer registered",
              ClassDB::hasClass("AnimationPlayer"));

  AnimationPlayer player;
  const eastl::string guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  player.setClipGuid("idle", guid);
  player.injectClipData(guid, make_test_clip("idle", 1.5f));
  player.play("idle");
  player.advance(0.25f);

  Variant playing;
  expect_true("is_playing property",
              ClassDB::getProperty(&player, "AnimationPlayer", "is_playing",
                                   playing));
  expect_true("is_playing true", playing.asBool());

  Variant position;
  expect_true("playback_position property",
              ClassDB::getProperty(&player, "AnimationPlayer",
                                   "playback_position", position));
  expect_true("playback_position value", float_near(position.asFloat(), 0.25f));

  Variant length;
  expect_true("clip_length property",
              ClassDB::getProperty(&player, "AnimationPlayer", "clip_length",
                                   length));
  expect_true("clip_length value", float_near(length.asFloat(), 1.5f));

  ClassDB::shutdown();
}

}  // namespace

int main() {
  test_clip_name_guid_map();
  test_play_stop_loop_and_advance();
  test_hard_cut_between_clips();
  test_two_slot_assignment_and_blend_weight();
  test_global_time_scale_default_and_set();
  test_dual_slot_weighted_blend_on_skeleton();
  test_single_slot_fallback();
  test_pose_applied_after_dual_slot_sample();
  test_play_with_zero_fade_is_hard_cut();
  test_crossfade_ramps_blend_weight_over_time();
  test_crossfade_blends_pose_mid_ramp();
  test_time_scale_scales_slot_advance();
  test_time_scale_advances_both_slots_via_blend_pose();
  test_time_scale_scales_phase1_advance();
  test_time_scale_scales_crossfade_ramp();
  test_playback_position_dominant_slot_by_weight();
  test_playback_position_crossfade_uses_target_slot();
  test_crossfade_from_phase1_single_clip();
  test_unknown_play_name_no_crash();
  test_object_hosts_animation_player();
  test_classdb_animation_player_registration();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
