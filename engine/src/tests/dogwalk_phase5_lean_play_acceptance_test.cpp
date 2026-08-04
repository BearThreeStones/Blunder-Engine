#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/animation_tree_asset.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/reflection/message_dispatch.h"
#include "runtime/function/script/animation_frame.h"
#include "runtime/resource/asset/asset_descriptor.h"

#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/gtc/quaternion.hpp>

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

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b, float eps = 1e-3f) {
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

void injectConstClip(Blunder::AnimationPlayer& player, const char* name,
                     const char* guid, const Blunder::Vec3& translation,
                     float duration = 2.0f) {
  Blunder::AnimationClipData clip;
  clip.name = name;
  clip.duration = duration;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", Blunder::AnimationInterpolation::Constant,
      {{0.0f, translation}, {duration, translation}}));
  player.setClipGuid(name, guid);
  player.injectClipData(guid, clip);
}

struct MessageSpy {
  std::vector<Blunder::MessageId> ids;
};

void message_hook(void* peer, Blunder::MessageId id,
                  const Blunder::MessageArg* /*args*/, int /*argc*/) {
  static_cast<MessageSpy*>(peer)->ids.push_back(id);
}

/// Lean Play A: visible LookAt post-pose + observable method dispatch under Play frame.
void test_lean_play_a() {
  using namespace Blunder;
  ObjectDB::clear();
  MessageDispatch::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("lean A object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  const int hips = skeleton->addBone("Hips", -1);
  const int head = skeleton->addBone("Head", hips);
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  auto look_at = eastl::make_unique<SkeletonLookAtModifier>();
  look_at->setBoneName("Head");
  look_at->setTarget(Vec3(1.0f, 0.0f, 1.0f));
  object->addSkeletonModifier(eastl::move(look_at));

  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  AnimationMethodKey key;
  key.name = "FootStep";
  key.time = 0.2f;
  clip.method_keys.push_back(key);
  player->setClipGuid("idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  player->injectClipData("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", clip);

  const MessageId foot_id = MessageDispatch::registerName("FootStep");
  MessageSpy spy;
  MessageDispatch::setHook(message_hook);
  object->setBehaviourScriptPeer(object->addBehaviour("Spy"), &spy);

  const Quat before =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("lean A play", player->play("idle"));
  expect_true(
      "lean A modifier visible",
      std::fabs(glm::dot(
          before, skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation)) <
          0.999f);

  spy.ids.clear();
  tickObjectAnimationPlayFrame(object, 0.25f, /*play_paused=*/false);
  expect_true("lean A method observed",
              spy.ids.size() == 1 && spy.ids[0] == foot_id);

  ObjectDB::clear();
  MessageDispatch::clear();
}

/// Lean Play C: perceptible BlendSpace2D (x,y) change under Play tick (test field).
void test_lean_play_c() {
  using namespace Blunder;
  ObjectDB::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("lean C object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  skeleton->addBone("Hips", -1);
  skeleton->resetPoseToRest();

  injectConstClip(*player, "idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                  Vec3(0.0f, 0.0f, 0.0f));
  injectConstClip(*player, "walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb",
                  Vec3(0.0f, 4.0f, 0.0f));
  injectConstClip(*player, "run", "cccccccc-cccc-cccc-cccc-cccccccccccc",
                  Vec3(4.0f, 0.0f, 0.0f));

  tree->addBlendSpace2DPoint("Locomotion2D", "idle", 0.0f, 0.0f);
  tree->addBlendSpace2DPoint("Locomotion2D", "walk", 1.0f, 0.0f);
  tree->addBlendSpace2DPoint("Locomotion2D", "run", 0.0f, 1.0f);
  tree->setStateBlendSpace2D("Move2D", "Locomotion2D");
  expect_true("lean C active", tree->setActive(true));
  expect_true("lean C travel", tree->travel("Move2D"));

  tree->setBlendSpace2DParam("Locomotion2D", 0.0f, 0.0f);
  tickObjectAnimationPlayFrame(object, 0.001f, /*play_paused=*/false);
  const Vec3 idle_pose = skeleton->getBonePoseLocal(0).translation;

  tree->setBlendSpace2DParam("Locomotion2D", 1.0f, 0.0f);
  tickObjectAnimationPlayFrame(object, 0.001f, /*play_paused=*/false);
  const Vec3 walk_pose = skeleton->getBonePoseLocal(0).translation;

  tree->setBlendSpace2DParam("Locomotion2D", 0.0f, 1.0f);
  tickObjectAnimationPlayFrame(object, 0.001f, /*play_paused=*/false);
  const Vec3 run_pose = skeleton->getBonePoseLocal(0).translation;

  expect_true("lean C idle pose", vec3_near(idle_pose, Vec3(0.0f, 0.0f, 0.0f)));
  expect_true("lean C walk pose", vec3_near(walk_pose, Vec3(0.0f, 4.0f, 0.0f)));
  expect_true("lean C run pose", vec3_near(run_pose, Vec3(4.0f, 0.0f, 0.0f)));
  expect_true("lean C idle!=walk", !vec3_near(idle_pose, walk_pose));
  expect_true("lean C idle!=run", !vec3_near(idle_pose, run_pose));

  ObjectDB::clear();
}

/// Lean Play D: Asset GUID + override drive pose without Behaviour Tick.
void test_lean_play_d() {
  using namespace Blunder;
  ObjectDB::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("lean D object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  skeleton->addBone("Hips", -1);
  skeleton->resetPoseToRest();

  injectConstClip(*player, "idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa",
                  Vec3(0.0f, 0.0f, 0.0f));
  injectConstClip(*player, "walk", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb",
                  Vec3(0.0f, 6.0f, 0.0f));

  AnimationTreeTopologyData topology;
  AnimationTreeTopologyData::BlendSpace1DDef space;
  space.node_name = "Locomotion";
  space.scalar = 0.0f;
  space.points.push_back({"idle", 0.0f});
  space.points.push_back({"walk", 1.0f});
  topology.blend_spaces_1d.push_back(eastl::move(space));
  topology.base_blend_space_node = "Locomotion";
  AnimationTreeTopologyData::StateDef state;
  state.name = "Move";
  state.kind = "blendSpace1D";
  state.blend_space_node = "Locomotion";
  topology.states.push_back(eastl::move(state));

  expect_true("lean D apply asset topology",
              applyAnimationTreeTopologyData(*tree, topology));
  tree->setAssetGuid("eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee");
  expect_true("lean D asset referenced", !tree->getAssetGuid().empty());

  AnimationTreeInstanceOverrides overrides;
  overrides.blend_space_scalars.push_back({"Locomotion", 1.0f});
  overrides.has_active = true;
  overrides.active = true;
  applyAnimationTreeInstanceOverrides(*tree, overrides);

  // No Behaviour Tick — override alone drives pose.
  expect_true("lean D override walk without behaviour",
              vec3_near(skeleton->getBonePoseLocal(0).translation,
                        Vec3(0.0f, 6.0f, 0.0f)));

  ObjectDB::clear();
}

}  // namespace

int main() {
  test_lean_play_a();
  test_lean_play_c();
  test_lean_play_d();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase5_lean_play_acceptance_test: all passed\n");
  return 0;
}
