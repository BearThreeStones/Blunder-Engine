#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/message_dispatch.h"

#include <cmath>
#include <cstdio>
#include <vector>

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

Blunder::AnimationTrack makeConstantTranslationTrack(
    const char* bone, float duration, const Blunder::Vec3& value) {
  Blunder::AnimationTrack track;
  track.bone = bone;
  track.channel = Blunder::AnimationChannel::Translation;
  track.interpolation = Blunder::AnimationInterpolation::Constant;
  Blunder::AnimationKeyframe start;
  start.time = 0.0f;
  start.value = {value.x, value.y, value.z};
  Blunder::AnimationKeyframe end;
  end.time = duration;
  end.value = {value.x, value.y, value.z};
  track.keys.push_back(start);
  track.keys.push_back(end);
  return track;
}

struct MessageSpy {
  std::vector<Blunder::MessageId> ids;
  std::vector<float> args;
};

void message_hook(void* peer, Blunder::MessageId id,
                  const Blunder::MessageArg* args, int argc) {
  auto* spy = static_cast<MessageSpy*>(peer);
  spy->ids.push_back(id);
  for (int index = 0; index < argc; ++index) {
    spy->args.push_back(args[index].f);
  }
}

Blunder::AnimationClipData makeClipWithMethodKeys(float duration) {
  Blunder::AnimationClipData clip;
  clip.name = "test";
  clip.duration = duration;
  clip.tracks.push_back(
      makeConstantTranslationTrack("Hips", duration, Blunder::Vec3(0.0f)));

  Blunder::AnimationMethodKey early;
  early.name = "Early";
  early.time = 0.25f;

  Blunder::AnimationMethodKey late;
  late.name = "Late";
  late.time = 0.75f;
  late.args.push_back(3.5f);

  clip.method_keys.push_back(early);
  clip.method_keys.push_back(late);
  return clip;
}

void bindClip(Blunder::AnimationPlayer& player, const char* name,
              const char* guid, const Blunder::AnimationClipData& clip) {
  player.setClipGuid(name, guid);
  player.injectClipData(guid, clip);
}

/// Task 2.1 YAML round-trip: asset_yaml_test::roundTripAnimationClipMethodKeys

/// Task 2.3: key-crossing dispatch on dominant-clip clock to co-located Behaviours.
void test_player_key_crossing_dispatch_via_message() {
  using namespace Blunder;

  ObjectDB::clear();
  MessageDispatch::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  object->ensureSkeleton()->addBone("Hips", -1);
  AnimationPlayer* player = object->ensureAnimationPlayer();

  const eastl::string guid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
  bindClip(*player, "idle", guid.c_str(), makeClipWithMethodKeys(1.0f));

  const MessageId early_id = MessageDispatch::registerName("Early");
  const MessageId late_id = MessageDispatch::registerName("Late");

  MessageSpy spy;
  MessageDispatch::setHook(message_hook);
  const BehaviourId behaviour_id = object->addBehaviour("Spy");
  object->setBehaviourScriptPeer(behaviour_id, &spy);

  expect_true("play", player->play("idle"));
  spy.ids.clear();
  spy.args.clear();
  player->advance(0.3f);
  expect_true("early key crossed", spy.ids.size() == 1);
  expect_true("early id", spy.ids.size() == 1 && spy.ids[0] == early_id);

  spy.ids.clear();
  spy.args.clear();
  player->advance(0.5f);
  expect_true("late key crossed", spy.ids.size() == 1);
  expect_true("late id", spy.ids.size() == 1 && spy.ids[0] == late_id);
  expect_true("late arg", spy.args.size() == 1 && float_near(spy.args[0], 3.5f));

  ObjectDB::clear();
  MessageDispatch::clear();
}

/// Task 2.4: while OneShot active, method dispatch uses OneShot clock.
void test_oneshot_method_dispatch_uses_oneshot_clock() {
  using namespace Blunder;

  ObjectDB::clear();
  MessageDispatch::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  object->ensureSkeleton()->addBone("Hips", -1);
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();

  const eastl::string base_guid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  const eastl::string shot_guid = "cccccccc-cccc-cccc-cccc-cccccccccccc";

  Blunder::AnimationClipData base_clip;
  base_clip.name = "base";
  base_clip.duration = 2.0f;
  base_clip.tracks.push_back(
      makeConstantTranslationTrack("Hips", 2.0f, Vec3(0.0f)));
  Blunder::AnimationMethodKey base_key;
  base_key.name = "BaseOnly";
  base_key.time = 1.0f;
  base_clip.method_keys.push_back(base_key);

  Blunder::AnimationClipData shot_clip;
  shot_clip.name = "shot";
  shot_clip.duration = 1.0f;
  shot_clip.tracks.push_back(
      makeConstantTranslationTrack("Hips", 1.0f, Vec3(1.0f)));
  Blunder::AnimationMethodKey shot_key;
  shot_key.name = "ShotOnly";
  shot_key.time = 0.4f;
  shot_clip.method_keys.push_back(shot_key);

  bindClip(*player, "base", base_guid.c_str(), base_clip);
  bindClip(*player, "shot", shot_guid.c_str(), shot_clip);

  const MessageId base_id = MessageDispatch::registerName("BaseOnly");
  const MessageId shot_id = MessageDispatch::registerName("ShotOnly");

  MessageSpy spy;
  MessageDispatch::setHook(message_hook);
  const BehaviourId behaviour_id = object->addBehaviour("Spy");
  object->setBehaviourScriptPeer(behaviour_id, &spy);

  tree->setSampleClipName("base");
  tree->setActive(true);
  tree->requestOneShot("shot");

  spy.ids.clear();
  tree->advance(0.5f);
  expect_true("oneshot key fired", spy.ids.size() == 1);
  expect_true("oneshot id", spy.ids.size() == 1 && spy.ids[0] == shot_id);
  expect_true("base key not fired at sample time",
              spy.ids.size() == 1 && spy.ids[0] != base_id);

  ObjectDB::clear();
  MessageDispatch::clear();
}

/// Task 2.5: product path uses MessageDispatch, not PtrCall (see animation_method_dispatch.cpp).
void test_method_dispatch_product_path_is_message_not_ptrcall() {
  using namespace Blunder;

  ObjectDB::clear();
  MessageDispatch::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  object->ensureSkeleton()->addBone("Hips", -1);
  AnimationPlayer* player = object->ensureAnimationPlayer();

  const eastl::string guid = "eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee";
  bindClip(*player, "idle", guid.c_str(), makeClipWithMethodKeys(1.0f));

  MessageSpy spy;
  MessageDispatch::setHook(message_hook);
  const BehaviourId behaviour_id = object->addBehaviour("Spy");
  object->setBehaviourScriptPeer(behaviour_id, &spy);

  expect_true("play", player->play("idle"));
  spy.ids.clear();
  player->advance(0.3f);
  expect_true("method track delivered via Message hook",
              spy.ids.size() == 1);

  ObjectDB::clear();
  MessageDispatch::clear();
}

}  // namespace

int main() {
  test_player_key_crossing_dispatch_via_message();
  test_oneshot_method_dispatch_uses_oneshot_clock();
  test_method_dispatch_product_path_is_message_not_ptrcall();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
