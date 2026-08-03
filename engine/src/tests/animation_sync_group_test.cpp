#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/resource/asset/asset_descriptor.h"

#include <cmath>
#include <cstdio>
#include <type_traits>

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

Blunder::AnimationClipData makeTranslationClip(const char* name, float duration,
                                               const Blunder::Vec3& translation) {
  Blunder::AnimationClipData clip;
  clip.name = name;
  clip.duration = duration;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", Blunder::AnimationInterpolation::Constant,
      {{0.0f, translation}, {duration, translation}}));
  return clip;
}

Blunder::AnimationClipData make_test_clip(const char* name, float duration) {
  Blunder::AnimationClipData clip;
  clip.name = name;
  clip.duration = duration;
  return clip;
}

void bind_clip(Blunder::AnimationPlayer& player, const char* clip_name,
               const char* guid, float duration) {
  eastl::string guid_str(guid);
  player.setClipGuid(clip_name, guid_str);
  player.injectClipData(guid_str, make_test_clip(clip_name, duration));
}

}  // namespace

void test_create_returns_valid_id() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  const SyncGroupId group = service.create();
  expect_true("create non-zero", group != k_invalid_sync_group_id);
  expect_true("empty group", service.getMemberCount(group) == 0);
}

void test_join_and_leave_members() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  const SyncGroupId group = service.create();

  expect_true("join player_a", service.join(group, &player_a));
  expect_true("member count one", service.getMemberCount(group) == 1);
  expect_true("player_a is member", service.isMember(group, &player_a));

  expect_true("join player_b", service.join(group, &player_b));
  expect_true("member count two", service.getMemberCount(group) == 2);
  expect_true("player_b is member", service.isMember(group, &player_b));

  expect_true("leave player_a", service.leave(group, &player_a));
  expect_true("member count after leave", service.getMemberCount(group) == 1);
  expect_true("player_a not member", !service.isMember(group, &player_a));
  expect_true("player_b still member", service.isMember(group, &player_b));
}

void test_destroy_releases_group() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId group = service.create();
  expect_true("join before destroy", service.join(group, &player));

  expect_true("destroy succeeds", service.destroy(group));
  expect_true("destroyed group has zero members",
              service.getMemberCount(group) == 0);
  expect_true("join fails after destroy", !service.join(group, &player));
  expect_true("leave fails after destroy", !service.leave(group, &player));
  expect_true("not member after destroy", !service.isMember(group, &player));
  expect_true("destroy again fails", !service.destroy(group));
}

void test_join_leave_validation() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId group = service.create();

  expect_true("null player join fails", !service.join(group, nullptr));
  expect_true("null player leave fails", !service.leave(group, nullptr));
  expect_true("invalid group join fails",
              !service.join(k_invalid_sync_group_id, &player));
  expect_true("invalid group leave fails",
              !service.leave(k_invalid_sync_group_id, &player));
  expect_true("leave non-member fails", !service.leave(group, &player));
  expect_true("duplicate join fails",
              service.join(group, &player) && !service.join(group, &player));
}

void test_player_may_join_multiple_groups() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId group_a = service.create();
  const SyncGroupId group_b = service.create();

  expect_true("join group_a", service.join(group_a, &player));
  expect_true("join group_b", service.join(group_b, &player));
  expect_true("member of group_a", service.isMember(group_a, &player));
  expect_true("member of group_b", service.isMember(group_b, &player));

  expect_true("leave group_a only", service.leave(group_a, &player));
  expect_true("not in group_a", !service.isMember(group_a, &player));
  expect_true("still in group_b", service.isMember(group_b, &player));
}

void test_destroy_invalid_and_unknown_ids() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  expect_true("destroy invalid id fails",
              !service.destroy(k_invalid_sync_group_id));

  const SyncGroupId live = service.create();
  expect_true("destroy unknown id fails", !service.destroy(live + 1000));
  expect_true("live group still usable", service.getMemberCount(live) == 0);
}

void test_join_unknown_group_id() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  const SyncGroupId live = service.create();

  expect_true("join unknown id fails", !service.join(live + 1000, &player));
  expect_true("leave unknown id fails", !service.leave(live + 1000, &player));
  expect_true("not member of unknown id",
              !service.isMember(live + 1000, &player));
  expect_true("unknown id member count zero",
              service.getMemberCount(live + 1000) == 0);
  expect_true("getMemberAt unknown id null",
              service.getMemberAt(live + 1000, 0) == nullptr);
}

void test_create_returns_distinct_ids() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  const SyncGroupId group_a = service.create();
  const SyncGroupId group_b = service.create();
  const SyncGroupId group_c = service.create();

  expect_true("ids non-zero",
              group_a != k_invalid_sync_group_id &&
                  group_b != k_invalid_sync_group_id &&
                  group_c != k_invalid_sync_group_id);
  expect_true("ids distinct",
              group_a != group_b && group_b != group_c && group_a != group_c);
}

void test_fire_heterogeneous_clips_hard_cut() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "CINE-character-attach",
            "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  bind_clip(player_b, "CINE-prop-attach",
            "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 3.0f);

  const SyncGroupId group = service.create();
  expect_true("join player_a", service.join(group, &player_a));
  expect_true("join player_b", service.join(group, &player_b));

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{&player_a, "CINE-character-attach"});
  instructions.push_back(SyncGroupFireInstruction{&player_b, "CINE-prop-attach"});

  expect_true("fire succeeds", service.fire(group, instructions));

  expect_true("player_a playing", player_a.isPlaying());
  expect_true("player_b playing", player_b.isPlaying());
  expect_true("player_a clip",
              player_a.getCurrentClipName() == "CINE-character-attach");
  expect_true("player_b clip",
              player_b.getCurrentClipName() == "CINE-prop-attach");
  expect_true("player_a position zero",
              float_near(player_a.getPlaybackPosition(), 0.0f));
  expect_true("player_b position zero",
              float_near(player_b.getPlaybackPosition(), 0.0f));
  expect_true("player_a not crossfading", !player_a.isCrossfading());
  expect_true("player_b not crossfading", !player_b.isCrossfading());
}

void test_fire_with_seek() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "clip_a", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 4.0f);
  bind_clip(player_b, "clip_b", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 5.0f);

  const SyncGroupId group = service.create();
  expect_true("join players", service.join(group, &player_a) &&
                                   service.join(group, &player_b));

  eastl::vector<SyncGroupFireInstruction> instructions;
  SyncGroupFireInstruction inst_a;
  inst_a.player = &player_a;
  inst_a.clip_name = "clip_a";
  inst_a.seek_seconds = 1.5f;
  inst_a.has_seek = true;
  instructions.push_back(inst_a);

  SyncGroupFireInstruction inst_b;
  inst_b.player = &player_b;
  inst_b.clip_name = "clip_b";
  inst_b.seek_seconds = 2.25f;
  inst_b.has_seek = true;
  instructions.push_back(inst_b);

  expect_true("fire with seek succeeds", service.fire(group, instructions));
  expect_true("player_a seek position",
              float_near(player_a.getPlaybackPosition(), 1.5f));
  expect_true("player_b seek position",
              float_near(player_b.getPlaybackPosition(), 2.25f));
}

void test_fire_hard_cut_interrupts_crossfade() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  bind_clip(player, "idle", "11111111-1111-1111-1111-111111111111", 2.0f);
  bind_clip(player, "walk", "22222222-2222-2222-2222-222222222222", 3.0f);

  const SyncGroupId group = service.create();
  expect_true("join player", service.join(group, &player));

  expect_true("crossfade to walk", player.play("walk", 1.0f));
  expect_true("crossfading", player.isCrossfading());
  player.advance(0.3f);

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{&player, "idle"});
  expect_true("fire hard cut", service.fire(group, instructions));

  expect_true("idle playing", player.getCurrentClipName() == "idle");
  expect_true("position reset", float_near(player.getPlaybackPosition(), 0.0f));
  expect_true("not crossfading", !player.isCrossfading());
  expect_true("blend weight cleared", float_near(player.getBlendWeight(), 0.0f));
  expect_true("slot0 cleared", player.getSlotClipName(0).empty());
  expect_true("slot1 cleared", player.getSlotClipName(1).empty());
}

void test_fire_clears_dual_slot_weighted_blend() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player;
  bind_clip(player, "idle", "11111111-1111-1111-1111-111111111111", 2.0f);
  bind_clip(player, "walk", "22222222-2222-2222-2222-222222222222", 3.0f);
  bind_clip(player, "cine", "33333333-3333-3333-3333-333333333333", 4.0f);

  const SyncGroupId group = service.create();
  expect_true("join player", service.join(group, &player));

  expect_true("slot0 idle", player.setSlot(0, "idle"));
  expect_true("slot1 walk", player.setSlot(1, "walk"));
  player.setBlendWeight(0.65f);
  expect_true("play idle", player.play("idle"));
  player.advance(0.4f);
  expect_true("dual slot active",
              !player.getSlotClipName(0).empty() &&
                  !player.getSlotClipName(1).empty());
  expect_true("blend weight mid-ramp", float_near(player.getBlendWeight(), 0.65f));

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{&player, "cine"});
  expect_true("fire snaps dual-slot blend", service.fire(group, instructions));

  expect_true("cine playing", player.getCurrentClipName() == "cine");
  expect_true("position reset", float_near(player.getPlaybackPosition(), 0.0f));
  expect_true("not crossfading", !player.isCrossfading());
  expect_true("blend weight cleared", float_near(player.getBlendWeight(), 0.0f));
  expect_true("slot0 cleared", player.getSlotClipName(0).empty());
  expect_true("slot1 cleared", player.getSlotClipName(1).empty());
}

void test_fire_clears_mid_crossfade_multiple_members() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  bind_clip(player_a, "walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 3.0f);
  bind_clip(player_a, "cine_a", "cccccccc-cccc-cccc-cccc-cccccccccccc", 2.5f);
  bind_clip(player_b, "idle", "dddddddd-dddd-dddd-dddd-dddddddddddd", 2.0f);
  bind_clip(player_b, "walk", "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee", 3.0f);
  bind_clip(player_b, "cine_b", "ffffffff-ffff-ffff-ffff-ffffffffffff", 3.5f);

  const SyncGroupId group = service.create();
  expect_true("join players", service.join(group, &player_a) &&
                                   service.join(group, &player_b));

  expect_true("player_a crossfade", player_a.play("walk", 1.0f));
  expect_true("player_b crossfade", player_b.play("walk", 0.8f));
  expect_true("player_a crossfading", player_a.isCrossfading());
  expect_true("player_b crossfading", player_b.isCrossfading());
  player_a.advance(0.25f);
  player_b.advance(0.35f);

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{&player_a, "cine_a"});
  instructions.push_back(SyncGroupFireInstruction{&player_b, "cine_b"});
  expect_true("fire heterogeneous hard cut", service.fire(group, instructions));

  expect_true("player_a cine", player_a.getCurrentClipName() == "cine_a");
  expect_true("player_b cine", player_b.getCurrentClipName() == "cine_b");
  expect_true("player_a position zero",
              float_near(player_a.getPlaybackPosition(), 0.0f));
  expect_true("player_b position zero",
              float_near(player_b.getPlaybackPosition(), 0.0f));
  expect_true("player_a not crossfading", !player_a.isCrossfading());
  expect_true("player_b not crossfading", !player_b.isCrossfading());
  expect_true("player_a blend cleared",
              float_near(player_a.getBlendWeight(), 0.0f));
  expect_true("player_b blend cleared",
              float_near(player_b.getBlendWeight(), 0.0f));
}

void test_fire_from_mid_playback() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "old_a", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  bind_clip(player_a, "new_a", "cccccccc-cccc-cccc-cccc-cccccccccccc", 2.5f);
  bind_clip(player_b, "old_b", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 2.0f);
  bind_clip(player_b, "new_b", "dddddddd-dddd-dddd-dddd-dddddddddddd", 3.0f);

  const SyncGroupId group = service.create();
  expect_true("join players", service.join(group, &player_a) &&
                                   service.join(group, &player_b));

  expect_true("play old_a", player_a.play("old_a"));
  expect_true("play old_b", player_b.play("old_b"));
  player_a.advance(0.8f);
  player_b.advance(1.1f);

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{&player_a, "new_a"});
  instructions.push_back(SyncGroupFireInstruction{&player_b, "new_b"});
  expect_true("fire new clips", service.fire(group, instructions));

  expect_true("player_a new clip", player_a.getCurrentClipName() == "new_a");
  expect_true("player_b new clip", player_b.getCurrentClipName() == "new_b");
  expect_true("player_a reset", float_near(player_a.getPlaybackPosition(), 0.0f));
  expect_true("player_b reset", float_near(player_b.getPlaybackPosition(), 0.0f));
}

void test_fire_validation() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer member;
  AnimationPlayer outsider;
  bind_clip(member, "clip", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 1.0f);

  const SyncGroupId group = service.create();
  expect_true("join member", service.join(group, &member));

  eastl::vector<SyncGroupFireInstruction> empty;
  expect_true("empty instructions fail", !service.fire(group, empty));
  expect_true("invalid group fail",
              !service.fire(k_invalid_sync_group_id, empty));

  eastl::vector<SyncGroupFireInstruction> null_player;
  null_player.push_back(SyncGroupFireInstruction{nullptr, "clip"});
  expect_true("null player fail", !service.fire(group, null_player));

  eastl::vector<SyncGroupFireInstruction> non_member;
  bind_clip(outsider, "clip", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 1.0f);
  non_member.push_back(SyncGroupFireInstruction{&outsider, "clip"});
  expect_true("non-member fail", !service.fire(group, non_member));

  eastl::vector<SyncGroupFireInstruction> unknown_clip;
  unknown_clip.push_back(SyncGroupFireInstruction{&member, "missing"});
  expect_true("unknown clip fail", !service.fire(group, unknown_clip));
}

void test_fire_atomic_on_resolve_failure() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "old_a", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  bind_clip(player_a, "new_a", "cccccccc-cccc-cccc-cccc-cccccccccccc", 2.5f);
  player_b.setClipGuid("clip_b", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");

  const SyncGroupId group = service.create();
  expect_true("join players", service.join(group, &player_a) &&
                                   service.join(group, &player_b));

  expect_true("play old_a", player_a.play("old_a"));
  player_a.advance(0.75f);

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{&player_a, "new_a"});
  instructions.push_back(SyncGroupFireInstruction{&player_b, "clip_b"});
  expect_true("fire fails when clip cannot resolve", !service.fire(group, instructions));

  expect_true("player_a unchanged clip", player_a.getCurrentClipName() == "old_a");
  expect_true("player_a unchanged position",
              float_near(player_a.getPlaybackPosition(), 0.75f));
  expect_true("player_b not playing", !player_b.isPlaying());
}

void test_fire_same_name_resolves_per_member_map() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "walk", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  bind_clip(player_b, "walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 3.0f);

  const SyncGroupId group = service.create();
  expect_true("join player_a", service.join(group, &player_a));
  expect_true("join player_b", service.join(group, &player_b));

  expect_true("fireSameName succeeds", service.fireSameName(group, "walk"));

  expect_true("player_a playing", player_a.isPlaying());
  expect_true("player_b playing", player_b.isPlaying());
  expect_true("player_a clip", player_a.getCurrentClipName() == "walk");
  expect_true("player_b clip", player_b.getCurrentClipName() == "walk");
  expect_true("player_a position zero",
              float_near(player_a.getPlaybackPosition(), 0.0f));
  expect_true("player_b position zero",
              float_near(player_b.getPlaybackPosition(), 0.0f));
  expect_true("player_a not crossfading", !player_a.isCrossfading());
  expect_true("player_b not crossfading", !player_b.isCrossfading());
}

void test_fire_same_name_clears_dual_slot_blend() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  bind_clip(player_a, "walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 3.0f);
  bind_clip(player_a, "sync", "cccccccc-cccc-cccc-cccc-cccccccccccc", 2.0f);
  bind_clip(player_b, "idle", "dddddddd-dddd-dddd-dddd-dddddddddddd", 2.0f);
  bind_clip(player_b, "walk", "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee", 3.0f);
  bind_clip(player_b, "sync", "ffffffff-ffff-ffff-ffff-ffffffffffff", 2.0f);

  const SyncGroupId group = service.create();
  expect_true("join players", service.join(group, &player_a) &&
                                   service.join(group, &player_b));

  expect_true("player_a slots", player_a.setSlot(0, "idle") &&
                                    player_a.setSlot(1, "walk"));
  expect_true("player_b slots", player_b.setSlot(0, "idle") &&
                                    player_b.setSlot(1, "walk"));
  player_a.setBlendWeight(0.4f);
  player_b.setBlendWeight(0.75f);
  expect_true("play player_a", player_a.play("idle"));
  expect_true("play player_b", player_b.play("idle"));
  player_a.advance(0.5f);
  player_b.advance(0.6f);

  expect_true("fireSameName hard cut", service.fireSameName(group, "sync"));

  expect_true("player_a sync", player_a.getCurrentClipName() == "sync");
  expect_true("player_b sync", player_b.getCurrentClipName() == "sync");
  expect_true("player_a not crossfading", !player_a.isCrossfading());
  expect_true("player_b not crossfading", !player_b.isCrossfading());
  expect_true("player_a blend cleared",
              float_near(player_a.getBlendWeight(), 0.0f));
  expect_true("player_b blend cleared",
              float_near(player_b.getBlendWeight(), 0.0f));
  expect_true("player_a slots cleared", player_a.getSlotClipName(0).empty() &&
                                           player_a.getSlotClipName(1).empty());
  expect_true("player_b slots cleared", player_b.getSlotClipName(0).empty() &&
                                           player_b.getSlotClipName(1).empty());
}

void test_fire_same_name_with_seek() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "clip", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 4.0f);
  bind_clip(player_b, "clip", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 5.0f);

  const SyncGroupId group = service.create();
  expect_true("join players", service.join(group, &player_a) &&
                                   service.join(group, &player_b));

  expect_true("fireSameName with seek succeeds",
              service.fireSameName(group, "clip", 1.75f));
  expect_true("player_a seek position",
              float_near(player_a.getPlaybackPosition(), 1.75f));
  expect_true("player_b seek position",
              float_near(player_b.getPlaybackPosition(), 1.75f));
}

void test_fire_same_name_validation() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer member;
  bind_clip(member, "clip", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 1.0f);

  const SyncGroupId group = service.create();
  expect_true("join member", service.join(group, &member));

  expect_true("invalid group fails",
              !service.fireSameName(k_invalid_sync_group_id, "clip"));
  expect_true("empty clip name fails", !service.fireSameName(group, ""));

  const SyncGroupId empty_group = service.create();
  expect_true("empty group fails", !service.fireSameName(empty_group, "clip"));

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  bind_clip(player_a, "walk", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  player_b.setClipGuid("walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb");

  const SyncGroupId mixed = service.create();
  expect_true("join mixed players", service.join(mixed, &player_a) &&
                                         service.join(mixed, &player_b));
  expect_true("play old clip", player_a.play("walk"));
  player_a.advance(0.5f);

  expect_true("resolve failure is atomic",
              !service.fireSameName(mixed, "walk"));
  expect_true("player_a unchanged",
              float_near(player_a.getPlaybackPosition(), 0.5f));
}

void test_sync_group_api_has_no_skeleton_parameters() {
  using namespace Blunder;

  static_assert(
      std::is_same_v<decltype(std::declval<SyncGroupFireInstruction>().player),
                     AnimationPlayer*>,
      "SyncGroupFireInstruction must reference AnimationPlayer only");
  static_assert(
      !std::is_invocable_v<decltype(&AnimationSyncGroupService::join),
                           AnimationSyncGroupService*, SyncGroupId, Skeleton*>,
      "Sync Group join must not accept Skeleton*");
  static_assert(
      !std::is_invocable_v<decltype(&AnimationSyncGroupService::fire),
                           AnimationSyncGroupService*, SyncGroupId,
                           eastl::vector<Skeleton*>&>,
      "Sync Group fire must not accept Skeleton* instructions");

  expect_true("fire instruction uses AnimationPlayer*",
              std::is_same_v<decltype(SyncGroupFireInstruction{}.player),
                             AnimationPlayer*>);
}

void test_fire_samples_only_colocated_skeletons() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  Skeleton skeleton_a = makeSingleBoneSkeleton("Hips");
  Skeleton skeleton_b = makeSingleBoneSkeleton("Hips");

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  player_a.bindSamplingSkeleton(&skeleton_a);
  player_b.bindSamplingSkeleton(&skeleton_b);

  const eastl::string guid_a = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  const eastl::string guid_b = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  player_a.setClipGuid("clip_a", guid_a);
  player_b.setClipGuid("clip_b", guid_b);
  player_a.injectClipData(guid_a, makeTranslationClip("clip_a", 1.0f,
                                                       Vec3(10.0f, 0.0f, 0.0f)));
  player_b.injectClipData(guid_b, makeTranslationClip("clip_b", 1.0f,
                                                       Vec3(20.0f, 0.0f, 0.0f)));

  const SyncGroupId group = service.create();
  expect_true("join player_a", service.join(group, &player_a));
  expect_true("join player_b", service.join(group, &player_b));

  skeleton_a.resetPoseToRest();
  skeleton_b.resetPoseToRest();
  expect_true("skeleton_a at rest",
              vec3_near(skeleton_a.getBonePoseLocal(0).translation,
                        Vec3(0.0f, 0.0f, 0.0f)));
  expect_true("skeleton_b at rest",
              vec3_near(skeleton_b.getBonePoseLocal(0).translation,
                        Vec3(0.0f, 0.0f, 0.0f)));

  eastl::vector<SyncGroupFireInstruction> instructions;
  instructions.push_back(SyncGroupFireInstruction{&player_a, "clip_a"});
  instructions.push_back(SyncGroupFireInstruction{&player_b, "clip_b"});
  expect_true("fire heterogeneous clips", service.fire(group, instructions));

  expect_true("player_a drives skeleton_a",
              vec3_near(skeleton_a.getBonePoseLocal(0).translation,
                        Vec3(10.0f, 0.0f, 0.0f)));
  expect_true("player_b drives skeleton_b",
              vec3_near(skeleton_b.getBonePoseLocal(0).translation,
                        Vec3(20.0f, 0.0f, 0.0f)));
  expect_true("skeleton_a not driven by player_b",
              !vec3_near(skeleton_a.getBonePoseLocal(0).translation,
                         Vec3(20.0f, 0.0f, 0.0f)));
  expect_true("skeleton_b not driven by player_a",
              !vec3_near(skeleton_b.getBonePoseLocal(0).translation,
                         Vec3(10.0f, 0.0f, 0.0f)));
}

void test_get_member_at_stable_insertion_order() {
  using namespace Blunder;

  AnimationSyncGroupService& service = animationSyncGroupService();
  service.clearAll();

  AnimationPlayer player_a;
  AnimationPlayer player_b;
  AnimationPlayer player_c;
  const SyncGroupId group = service.create();

  expect_true("join order a", service.join(group, &player_a));
  expect_true("join order b", service.join(group, &player_b));
  expect_true("join order c", service.join(group, &player_c));
  expect_true("member count three", service.getMemberCount(group) == 3);

  expect_true("index 0 is player_a",
              service.getMemberAt(group, 0) == &player_a);
  expect_true("index 1 is player_b",
              service.getMemberAt(group, 1) == &player_b);
  expect_true("index 2 is player_c",
              service.getMemberAt(group, 2) == &player_c);
  expect_true("out of range null", service.getMemberAt(group, 3) == nullptr);
}

int main() {
  test_create_returns_valid_id();
  test_join_and_leave_members();
  test_destroy_releases_group();
  test_join_leave_validation();
  test_player_may_join_multiple_groups();
  test_destroy_invalid_and_unknown_ids();
  test_join_unknown_group_id();
  test_create_returns_distinct_ids();
  test_fire_heterogeneous_clips_hard_cut();
  test_fire_with_seek();
  test_fire_hard_cut_interrupts_crossfade();
  test_fire_clears_dual_slot_weighted_blend();
  test_fire_clears_mid_crossfade_multiple_members();
  test_fire_from_mid_playback();
  test_fire_validation();
  test_fire_atomic_on_resolve_failure();
  test_fire_same_name_resolves_per_member_map();
  test_fire_same_name_clears_dual_slot_blend();
  test_fire_same_name_with_seek();
  test_fire_same_name_validation();
  test_sync_group_api_has_no_skeleton_parameters();
  test_fire_samples_only_colocated_skeletons();
  test_get_member_at_stable_insertion_order();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
