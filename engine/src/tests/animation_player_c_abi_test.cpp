#include "runtime/core/object/animation_player.h"
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
int g_pose_applied_hits = 0;
BlunderObjectId g_pose_applied_object_id = 0;

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

void on_pose_applied_c_abi(BlunderObjectId object_id, void* /*userdata*/) {
  ++g_pose_applied_hits;
  g_pose_applied_object_id = object_id;
}

void setup_object_with_clip(Blunder::ObjectId id, Blunder::Object* object,
                            const char* clip_name, const char* guid_str,
                            float duration) {
  Blunder::Skeleton* skeleton = object->ensureSkeleton();
  Blunder::AnimationPlayer* player = object->ensureAnimationPlayer();
  skeleton->addBone("Hips", -1);

  Blunder::AnimationClipData clip;
  clip.duration = duration;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", Blunder::AnimationInterpolation::Linear,
      {{0.0f, Blunder::Vec3(0.0f, 0.0f, 0.0f)},
       {duration, Blunder::Vec3(10.0f, 0.0f, 0.0f)}}));

  const eastl::string guid(guid_str);
  player->setClipGuid(clip_name, guid);
  player->injectClipData(guid, clip);
}

}  // namespace

int main() {
  using namespace Blunder;

  ObjectDB::clear();
  ClassDB::initialize();

  expect_true("abi version >= 7", blunder_engine_abi_version() >= 7);

  BlunderNativeAbi abi{};
  blunder_native_abi_fill_from_process(&abi);
  expect_true("abi animation play", abi.animation_player_play != nullptr);
  expect_true("abi animation play with fade",
              abi.animation_player_play_with_fade != nullptr);
  expect_true("abi animation set slot", abi.animation_player_set_slot != nullptr);
  expect_true("abi animation get slot", abi.animation_player_get_slot != nullptr);
  expect_true("abi animation set blend weight",
              abi.animation_player_set_blend_weight != nullptr);
  expect_true("abi animation get blend weight",
              abi.animation_player_get_blend_weight != nullptr);
  expect_true("abi animation set time scale",
              abi.animation_player_set_time_scale != nullptr);
  expect_true("abi animation get time scale",
              abi.animation_player_get_time_scale != nullptr);
  expect_true("abi animation stop", abi.animation_player_stop != nullptr);
  expect_true("abi pose listener", abi.animation_player_add_pose_applied_listener !=
                                       nullptr);

  const ObjectId native_id = ObjectDB::create();
  const BlunderObjectId id = static_cast<BlunderObjectId>(native_id);
  Object* object = ObjectDB::get(native_id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    ClassDB::shutdown();
    ObjectDB::clear();
    return 1;
  }

  setup_object_with_clip(native_id, object, "walk",
                         "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  setup_object_with_clip(native_id, object, "idle",
                         "11111111-1111-1111-1111-111111111111", 1.0f);

  expect_true("set slot0 idle",
              blunder_animation_player_set_slot(id, 0, "idle") ==
                  BLUNDER_ENGINE_OK);
  expect_true("set slot1 walk",
              blunder_animation_player_set_slot(id, 1, "walk") ==
                  BLUNDER_ENGINE_OK);
  char slot_name[64] = {};
  expect_true("get slot0",
              blunder_animation_player_get_slot(id, 0, slot_name,
                                              static_cast<int>(sizeof(slot_name))) ==
                  BLUNDER_ENGINE_OK);
  expect_true("slot0 name", std::strcmp(slot_name, "idle") == 0);
  expect_true("get slot1",
              blunder_animation_player_get_slot(id, 1, slot_name,
                                              static_cast<int>(sizeof(slot_name))) ==
                  BLUNDER_ENGINE_OK);
  expect_true("slot1 name", std::strcmp(slot_name, "walk") == 0);
  expect_true("unknown slot fails",
              blunder_animation_player_set_slot(id, 0, "missing") ==
                  BLUNDER_ENGINE_ERROR);

  expect_true("set blend weight",
              blunder_animation_player_set_blend_weight(id, 0.75f) ==
                  BLUNDER_ENGINE_OK);
  float blend_weight = 0.0f;
  expect_true("get blend weight",
              blunder_animation_player_get_blend_weight(id, &blend_weight) ==
                  BLUNDER_ENGINE_OK);
  expect_true("blend weight value", float_near(blend_weight, 0.75f));

  expect_true("set time scale",
              blunder_animation_player_set_time_scale(id, 2.0f) ==
                  BLUNDER_ENGINE_OK);
  float time_scale = 0.0f;
  expect_true("get time scale",
              blunder_animation_player_get_time_scale(id, &time_scale) ==
                  BLUNDER_ENGINE_OK);
  expect_true("time scale value", float_near(time_scale, 2.0f));

  expect_true("reset blend for fade",
              blunder_animation_player_set_blend_weight(id, 0.0f) ==
                  BLUNDER_ENGINE_OK);
  expect_true("play with fade via c abi",
              blunder_animation_player_play_with_fade(id, "walk", 0.5f) ==
                  BLUNDER_ENGINE_OK);
  expect_true("crossfading",
              object->getAnimationPlayer()->isCrossfading());
  object->getAnimationPlayer()->advance(0.25f);
  expect_true("get blend weight after fade",
              blunder_animation_player_get_blend_weight(id, &blend_weight) ==
                  BLUNDER_ENGINE_OK);
  expect_true("fade ramp complete", float_near(blend_weight, 1.0f, 1e-3f));

  expect_true("reset time scale for legacy play",
              blunder_animation_player_set_time_scale(id, 1.0f) ==
                  BLUNDER_ENGINE_OK);
  expect_true("stop before legacy play",
              blunder_animation_player_stop(id) == BLUNDER_ENGINE_OK);

  g_pose_applied_hits = 0;
  g_pose_applied_object_id = 0;
  expect_true(
      "add pose listener",
      blunder_animation_player_add_pose_applied_listener(id, on_pose_applied_c_abi,
                                                         nullptr) ==
          BLUNDER_ENGINE_OK);

  expect_true("play via c abi",
              blunder_animation_player_play(id, "walk") == BLUNDER_ENGINE_OK);
  expect_true("pose applied on play", g_pose_applied_hits == 1);
  expect_true("pose applied object id", g_pose_applied_object_id == id);

  int playing = 0;
  expect_true("is_playing property",
              blunder_object_get_bool_property(id, "AnimationPlayer", "is_playing",
                                               &playing) == BLUNDER_ENGINE_OK);
  expect_true("is playing", playing == 1);

  float position = 0.0f;
  float length = 0.0f;
  expect_true("get position",
              blunder_animation_player_get_playback_position(id, &position) ==
                  BLUNDER_ENGINE_OK);
  expect_true("position zero", float_near(position, 0.0f));
  expect_true("get length",
              blunder_animation_player_get_clip_length(id, &length) ==
                  BLUNDER_ENGINE_OK);
  expect_true("length two", float_near(length, 2.0f));

  expect_true("set loop",
              blunder_animation_player_set_loop(id, 1) == BLUNDER_ENGINE_OK);
  int looping = 0;
  expect_true("get loop property",
              blunder_object_get_bool_property(id, "AnimationPlayer", "is_looping",
                                               &looping) == BLUNDER_ENGINE_OK);
  expect_true("looping", looping == 1);

  object->getAnimationPlayer()->advance(0.5f);
  expect_true("get position after advance",
              blunder_animation_player_get_playback_position(id, &position) ==
                  BLUNDER_ENGINE_OK);
  expect_true("position half", float_near(position, 0.5f));

  expect_true("stop",
              blunder_animation_player_stop(id) == BLUNDER_ENGINE_OK);
  expect_true("get position after stop",
              blunder_animation_player_get_playback_position(id, &position) ==
                  BLUNDER_ENGINE_OK);
  expect_true("position reset", float_near(position, 0.0f));
  expect_true("not playing after stop",
              blunder_object_get_bool_property(id, "AnimationPlayer", "is_playing",
                                               &playing) == BLUNDER_ENGINE_OK &&
                  playing == 0);

  expect_true("unknown play fails",
              blunder_animation_player_play(id, "missing") == BLUNDER_ENGINE_ERROR);

  expect_true("clear pose listeners",
              blunder_animation_player_clear_pose_applied_listeners(id) ==
                  BLUNDER_ENGINE_OK);
  g_pose_applied_hits = 0;
  expect_true("play after clear listeners",
              blunder_animation_player_play(id, "walk") == BLUNDER_ENGINE_OK);
  expect_true("no pose after clear", g_pose_applied_hits == 0);

  // Destroy with active PoseApplied listener — bindings must be removed (no leak / dangling hook).
  g_pose_applied_hits = 0;
  expect_true(
      "re-add pose listener before destroy",
      blunder_animation_player_add_pose_applied_listener(id, on_pose_applied_c_abi,
                                                         nullptr) ==
          BLUNDER_ENGINE_OK);
  expect_true("destroy with pose listener",
              blunder_object_destroy(id) == BLUNDER_ENGINE_OK);
  expect_true("destroyed object invalid", blunder_object_is_valid(id) == 0);

  const ObjectId second_id = ObjectDB::create();
  const BlunderObjectId second_abi_id = static_cast<BlunderObjectId>(second_id);
  Object* second_object = ObjectDB::get(second_id);
  expect_true("second object created", second_object != nullptr);
  if (second_object != nullptr) {
    setup_object_with_clip(second_id, second_object, "walk",
                           "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 1.0f);
    g_pose_applied_hits = 0;
    g_pose_applied_object_id = 0;
    expect_true(
        "second add pose listener",
        blunder_animation_player_add_pose_applied_listener(
            second_abi_id, on_pose_applied_c_abi, nullptr) == BLUNDER_ENGINE_OK);
    expect_true("second play via c abi",
                blunder_animation_player_play(second_abi_id, "walk") ==
                    BLUNDER_ENGINE_OK);
    expect_true("second pose applied on play", g_pose_applied_hits == 1);
    expect_true("second pose applied object id",
                g_pose_applied_object_id == second_abi_id);
    second_object->getAnimationPlayer()->advance(0.25f);
    expect_true("second pose after advance", g_pose_applied_hits == 2);
    expect_true("table play second",
                abi.animation_player_play(second_abi_id, "walk") ==
                    BLUNDER_ENGINE_OK);
    ObjectDB::destroy(second_id);
  }

  expect_true("play invalid object after destroy",
              blunder_animation_player_play(id, "walk") == BLUNDER_ENGINE_ERROR);
  expect_true("table play invalid object",
              abi.animation_player_play(id, "walk") == BLUNDER_ENGINE_ERROR);

  ClassDB::shutdown();
  ObjectDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
