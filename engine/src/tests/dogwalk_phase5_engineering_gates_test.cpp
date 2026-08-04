#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/animation_tree_asset.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/reflection/message_dispatch.h"
#include "runtime/function/editor/animation_preview_controller.h"
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
                     float duration = 1.0f) {
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

/// Gate A engineering: LookAt post-pose + method key-crossing under Play tick.
void test_gate_a_modifier_and_method() {
  using namespace Blunder;
  ObjectDB::clear();
  MessageDispatch::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("gate A object", object != nullptr);
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
  look_at->setTarget(Vec3(1.0f, 1.0f, 1.0f));
  object->addSkeletonModifier(eastl::move(look_at));

  AnimationClipData clip;
  clip.duration = 1.0f;
  clip.tracks.push_back(makeTranslationTrack(
      "Hips", AnimationInterpolation::Constant,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}, {1.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  AnimationMethodKey key;
  key.name = "FootStep";
  key.time = 0.25f;
  clip.method_keys.push_back(key);
  player->setClipGuid("idle", "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  player->injectClipData("aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", clip);

  const MessageId foot_id = MessageDispatch::registerName("FootStep");
  MessageSpy spy;
  MessageDispatch::setHook(message_hook);
  const BehaviourId behaviour_id = object->addBehaviour("Spy");
  object->setBehaviourScriptPeer(behaviour_id, &spy);

  const Quat before =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("gate A play", player->play("idle"));
  const Quat after =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("gate A look-at changed rotation",
              std::fabs(glm::dot(before, after)) < 0.999f);

  spy.ids.clear();
  tickObjectAnimationPlayFrame(object, 0.3f, /*play_paused=*/false);
  expect_true("gate A method dispatched",
              spy.ids.size() == 1 && spy.ids[0] == foot_id);

  ObjectDB::clear();
  MessageDispatch::clear();
}

/// Gate C engineering: BlendSpace2D barycentric under named (x,y) API.
void test_gate_c_blend_space_2d() {
  using namespace Blunder;
  ObjectDB::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("gate C object", object != nullptr);
  if (object == nullptr) {
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();
  skeleton->addBone("Hips", -1);
  skeleton->resetPoseToRest();

  injectConstClip(*player, "a", "11111111-1111-1111-1111-111111111111",
                  Vec3(0.0f, 0.0f, 0.0f));
  injectConstClip(*player, "b", "22222222-2222-2222-2222-222222222222",
                  Vec3(10.0f, 0.0f, 0.0f));
  injectConstClip(*player, "c", "33333333-3333-3333-3333-333333333333",
                  Vec3(0.0f, 10.0f, 0.0f));

  expect_true("gate C add a",
              tree->addBlendSpace2DPoint("Locomotion2D", "a", 0.0f, 0.0f));
  expect_true("gate C add b",
              tree->addBlendSpace2DPoint("Locomotion2D", "b", 1.0f, 0.0f));
  expect_true("gate C add c",
              tree->addBlendSpace2DPoint("Locomotion2D", "c", 0.0f, 1.0f));
  expect_true("gate C base", tree->setBaseBlendSpace2DNode("Locomotion2D"));
  tree->setBlendSpace2DParam("Locomotion2D", 1.0f / 3.0f, 1.0f / 3.0f);
  expect_true("gate C active", tree->setActive(true));
  expect_true(
      "gate C centroid pose",
      vec3_near(skeleton->getBonePoseLocal(0).translation,
                Vec3(10.0f / 3.0f, 10.0f / 3.0f, 0.0f)));

  ObjectDB::clear();
}

/// Gate D engineering: Asset topology + instance override without Behaviour Tick.
void test_gate_d_asset_and_override() {
  using namespace Blunder;
  ObjectDB::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("gate D object", object != nullptr);
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

  AnimationPreviewController preview;
  preview.bindObject(object, "idle");
  expect_true("gate D apply topology", preview.applyTreeTopology(topology));
  preview.setAssetGuid("dddddddd-dddd-dddd-dddd-dddddddddddd");
  expect_true("gate D asset guid",
              tree->getAssetGuid() == "dddddddd-dddd-dddd-dddd-dddddddddddd");

  AnimationTreeInstanceOverrides overrides;
  overrides.blend_space_scalars.push_back({"Locomotion", 1.0f});
  overrides.has_active = true;
  overrides.active = true;
  preview.applyTreeOverrides(overrides);
  expect_true("gate D override walk pose",
              vec3_near(skeleton->getBonePoseLocal(0).translation,
                        Vec3(0.0f, 4.0f, 0.0f)));

  ObjectDB::clear();
}

}  // namespace

int main() {
  test_gate_a_modifier_and_method();
  test_gate_c_blend_space_2d();
  test_gate_d_asset_and_override();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase5_engineering_gates_test: all passed\n");
  return 0;
}
