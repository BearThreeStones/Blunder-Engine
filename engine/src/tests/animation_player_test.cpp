#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object_db.h"
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
  test_unknown_play_name_no_crash();
  test_object_hosts_animation_player();
  test_classdb_animation_player_registration();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
