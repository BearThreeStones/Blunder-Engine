#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sync_group.h"
#include "runtime/core/object/cine_segment_service.h"
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

void bind_clip(Blunder::AnimationPlayer& player, const char* clip_name,
               const char* guid, float duration) {
  eastl::string guid_str(guid);
  player.setClipGuid(clip_name, guid_str);
  Blunder::AnimationClipData clip;
  clip.name = clip_name;
  clip.duration = duration;
  player.injectClipData(guid_str, clip);
}

void setup_object_with_clip(Blunder::ObjectId id, Blunder::Object* object,
                            const char* clip_name, const char* guid_str,
                            float duration) {
  object->ensureSkeleton()->addBone("Hips", -1);
  bind_clip(*object->ensureAnimationPlayer(), clip_name, guid_str, duration);
}

}  // namespace

int main() {
  using namespace Blunder;

  ObjectDB::clear();
  ClassDB::initialize();
  animationSyncGroupService().clearAll();
  cineSegmentService().resetForTests();

  expect_true("abi version >= 8", blunder_engine_abi_version() >= 8);
  expect_true("abi version is 8", blunder_engine_abi_version() == 8);

  BlunderNativeAbi abi{};
  blunder_native_abi_fill_from_process(&abi);
  expect_true("abi sync group create", abi.sync_group_create != nullptr);
  expect_true("abi sync group destroy", abi.sync_group_destroy != nullptr);
  expect_true("abi sync group join", abi.sync_group_join != nullptr);
  expect_true("abi sync group leave", abi.sync_group_leave != nullptr);
  expect_true("abi sync group fire", abi.sync_group_fire != nullptr);
  expect_true("abi sync group fire same name",
              abi.sync_group_fire_same_name != nullptr);
  expect_true("abi sync group fire same name seek",
              abi.sync_group_fire_same_name_seek != nullptr);
  expect_true("abi cine enter", abi.cine_enter != nullptr);
  expect_true("abi cine end", abi.cine_end != nullptr);
  expect_true("abi cine is in cine", abi.cine_is_in_cine != nullptr);
  expect_true("abi cine suppress query",
              abi.cine_is_gameplay_input_suppressed != nullptr);

  const ObjectId object_a = ObjectDB::create();
  const ObjectId object_b = ObjectDB::create();
  const BlunderObjectId abi_a = static_cast<BlunderObjectId>(object_a);
  const BlunderObjectId abi_b = static_cast<BlunderObjectId>(object_b);
  Object* native_a = ObjectDB::get(object_a);
  Object* native_b = ObjectDB::get(object_b);
  expect_true("object_a created", native_a != nullptr);
  expect_true("object_b created", native_b != nullptr);
  if (native_a == nullptr || native_b == nullptr) {
    ClassDB::shutdown();
    ObjectDB::clear();
    return 1;
  }

  setup_object_with_clip(object_a, native_a, "CINE-character-attach",
                         "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", 2.0f);
  setup_object_with_clip(object_b, native_b, "CINE-prop-attach",
                         "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb", 3.0f);
  bind_clip(*native_a->ensureAnimationPlayer(), "walk",
            "cccccccc-cccc-cccc-cccc-cccccccccccc", 1.0f);
  bind_clip(*native_b->ensureAnimationPlayer(), "walk",
            "dddddddd-dddd-dddd-dddd-dddddddddddd", 1.5f);

  const BlunderSyncGroupId group = blunder_sync_group_create();
  expect_true("create non-zero", group != 0u);
  expect_true("join object_a",
              blunder_sync_group_join(group, abi_a) == BLUNDER_ENGINE_OK);
  expect_true("join object_b",
              blunder_sync_group_join(group, abi_b) == BLUNDER_ENGINE_OK);
  expect_true("join invalid object fails",
              blunder_sync_group_join(group, 999999u) == BLUNDER_ENGINE_ERROR);

  BlunderSyncGroupFireInstruction instructions[2]{};
  instructions[0].player_object_id = abi_a;
  instructions[0].clip_name = "CINE-character-attach";
  instructions[0].has_seek = 0;
  instructions[1].player_object_id = abi_b;
  instructions[1].clip_name = "CINE-prop-attach";
  instructions[1].has_seek = 0;

  expect_true("fire heterogeneous",
              blunder_sync_group_fire(group, instructions, 2) ==
                  BLUNDER_ENGINE_OK);
  expect_true("player_a playing", native_a->getAnimationPlayer()->isPlaying());
  expect_true("player_b playing", native_b->getAnimationPlayer()->isPlaying());
  expect_true("player_a clip",
              native_a->getAnimationPlayer()->getCurrentClipName() ==
                  "CINE-character-attach");
  expect_true("player_a not crossfading",
              !native_a->getAnimationPlayer()->isCrossfading());

  native_a->getAnimationPlayer()->stop();
  native_b->getAnimationPlayer()->stop();

  expect_true("fire same name",
              blunder_sync_group_fire_same_name(group, "walk") ==
                  BLUNDER_ENGINE_OK);
  expect_true("same name player_a playing",
              native_a->getAnimationPlayer()->isPlaying());
  expect_true("same name player_b playing",
              native_b->getAnimationPlayer()->isPlaying());

  native_a->getAnimationPlayer()->stop();
  native_b->getAnimationPlayer()->stop();

  instructions[0].clip_name = "walk";
  instructions[0].has_seek = 1;
  instructions[0].seek_seconds = 0.25f;
  instructions[1].player_object_id = abi_b;
  instructions[1].clip_name = "walk";
  instructions[1].has_seek = 1;
  instructions[1].seek_seconds = 0.25f;
  expect_true("fire with seek",
              blunder_sync_group_fire(group, instructions, 2) ==
                  BLUNDER_ENGINE_OK);
  expect_true("seek position a",
              float_near(native_a->getAnimationPlayer()->getPlaybackPosition(),
                         0.25f));

  expect_true("leave object_a",
              blunder_sync_group_leave(group, abi_a) == BLUNDER_ENGINE_OK);
  expect_true("leave non-member fails",
              blunder_sync_group_leave(group, abi_a) == BLUNDER_ENGINE_ERROR);
  expect_true("destroy group",
              blunder_sync_group_destroy(group) == BLUNDER_ENGINE_OK);
  expect_true("destroy again fails",
              blunder_sync_group_destroy(group) == BLUNDER_ENGINE_ERROR);

  expect_true("table create",
              abi.sync_group_create() != 0u);

  int in_cine = 1;
  int suppressed = 1;
  expect_true("not in cine initially",
              blunder_cine_is_in_cine(&in_cine) == BLUNDER_ENGINE_OK &&
                  in_cine == 0);
  expect_true("enter without suppress",
              blunder_cine_enter(0) == BLUNDER_ENGINE_OK);
  expect_true("in cine after enter",
              blunder_cine_is_in_cine(&in_cine) == BLUNDER_ENGINE_OK &&
                  in_cine == 1);
  expect_true("not suppressed without flag",
              blunder_cine_is_gameplay_input_suppressed(&suppressed) ==
                  BLUNDER_ENGINE_OK &&
                  suppressed == 0);
  expect_true("end clears mark", blunder_cine_end() == BLUNDER_ENGINE_OK);
  expect_true("not in cine after end",
              blunder_cine_is_in_cine(&in_cine) == BLUNDER_ENGINE_OK &&
                  in_cine == 0);
  expect_true("end without enter fails",
              blunder_cine_end() == BLUNDER_ENGINE_ERROR);

  expect_true("enter with suppress",
              blunder_cine_enter(1) == BLUNDER_ENGINE_OK);
  expect_true("suppressed while in cine",
              blunder_cine_is_gameplay_input_suppressed(&suppressed) ==
                  BLUNDER_ENGINE_OK &&
                  suppressed == 1);
  expect_true("table end", abi.cine_end() == BLUNDER_ENGINE_OK);
  expect_true("suppression cleared after end",
              blunder_cine_is_gameplay_input_suppressed(&suppressed) ==
                  BLUNDER_ENGINE_OK &&
                  suppressed == 0);

  cineSegmentService().resetForTests();
  ObjectDB::destroy(object_a);
  ObjectDB::destroy(object_b);
  ClassDB::shutdown();
  ObjectDB::clear();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
