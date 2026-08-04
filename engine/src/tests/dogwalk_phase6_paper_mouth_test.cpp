#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_preview_controller.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;
int g_tick_calls = 0;

void on_tick(void* /*peer*/, float /*dt*/) { ++g_tick_calls; }

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

/// Task 2.1: openAmount 0 vs positive produces detectable jaw local pose change.
void test_paper_mouth_open_amount_changes_jaw_pose() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("PaperMouth registered", ClassDB::hasClass("PaperMouth"));

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    ClassDB::shutdown();
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  const int head = skeleton->addBone("Head", -1);
  const int jaw = skeleton->addBone("Jaw", head);
  skeleton->setBoneRestLocal(static_cast<size_t>(jaw),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.2f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  SkeletonPaperMouthModifier* paper_mouth = object->addSkeletonPaperMouthModifier();
  expect_true("paper mouth created", paper_mouth != nullptr);
  if (paper_mouth == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  expect_true("open_amount closed via ClassDB",
              ClassDB::setProperty(paper_mouth, "PaperMouth", "open_amount",
                                   Variant(0.0f)));
  skeleton->resetPoseToRest();
  paper_mouth->apply(*skeleton);
  const Quat jaw_rotation_closed =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;

  expect_true("open_amount open via ClassDB",
              ClassDB::setProperty(paper_mouth, "PaperMouth", "open_amount",
                                   Variant(1.0f)));
  skeleton->resetPoseToRest();
  paper_mouth->apply(*skeleton);
  const Quat jaw_rotation_open =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;

  expect_true("open jaw rotation differs from closed",
              std::fabs(glm::dot(jaw_rotation_closed, jaw_rotation_open)) <
                  0.999f);

  Variant open_amount;
  expect_true("open_amount get via ClassDB",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "open_amount",
                                   open_amount));
  expect_true("open_amount round-trip", float_near(open_amount.asFloat(), 1.0f));

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 2.2: bone_name switches which bone openAmount affects.
void test_paper_mouth_configurable_bone_name() {
  using namespace Blunder;

  ClassDB::initialize();

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    ClassDB::shutdown();
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  const int head = skeleton->addBone("Head", -1);
  const int jaw = skeleton->addBone("Jaw", head);
  skeleton->setBoneRestLocal(static_cast<size_t>(jaw),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.2f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  SkeletonPaperMouthModifier* paper_mouth = object->addSkeletonPaperMouthModifier();
  expect_true("paper mouth created", paper_mouth != nullptr);
  if (paper_mouth == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  expect_true("open_amount open via ClassDB",
              ClassDB::setProperty(paper_mouth, "PaperMouth", "open_amount",
                                   Variant(1.0f)));

  Variant bone_name;
  expect_true("default bone_name get via ClassDB",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "bone_name",
                                   bone_name));
  expect_true("default bone_name is Jaw", bone_name.asString() == "Jaw");

  skeleton->resetPoseToRest();
  paper_mouth->apply(*skeleton);
  const Quat jaw_rotation_default_bone =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  const Quat head_rotation_before_switch =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("default Jaw bone rotates when open",
              std::fabs(glm::dot(jaw_rotation_default_bone,
                                 glm::identity<Quat>())) < 0.999f);

  expect_true("bone_name switched to Head via ClassDB",
              ClassDB::setProperty(paper_mouth, "PaperMouth", "bone_name",
                                   Variant(eastl::string("Head"))));
  expect_true("bone_name get after switch",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "bone_name",
                                   bone_name));
  expect_true("bone_name round-trip Head", bone_name.asString() == "Head");

  skeleton->resetPoseToRest();
  const Quat jaw_rotation_before =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  paper_mouth->apply(*skeleton);
  const Quat jaw_rotation_after =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  const Quat head_rotation_after =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("Head rotates after bone_name switch",
              std::fabs(glm::dot(head_rotation_before_switch,
                                 head_rotation_after)) < 0.999f);
  expect_true("Jaw rotation unchanged when targeting Head",
              std::fabs(glm::dot(jaw_rotation_before, jaw_rotation_after)) >
                  0.999f);

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 2.3: optional attach-driven mode fills openAmount when enabled; default off.
void test_paper_mouth_attach_driven_open_amount() {
  using namespace Blunder;

  ClassDB::initialize();

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    ClassDB::shutdown();
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  const int head = skeleton->addBone("Head", -1);
  const int jaw = skeleton->addBone("Jaw", head);
  skeleton->setBoneRestLocal(static_cast<size_t>(jaw),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.2f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  SkeletonPaperMouthModifier* paper_mouth = object->addSkeletonPaperMouthModifier();
  expect_true("paper mouth created", paper_mouth != nullptr);
  if (paper_mouth == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  Variant attach_driven;
  expect_true("attach_driven get via ClassDB",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "attach_driven",
                                   attach_driven));
  expect_true("default attach_driven off", !attach_driven.asBool());

  expect_true("open_amount explicit via ClassDB",
              ClassDB::setProperty(paper_mouth, "PaperMouth", "open_amount",
                                   Variant(0.25f)));

  paper_mouth->setAttachOccupancy(0.75f);
  Variant open_amount;
  expect_true("open_amount get after occupancy while off",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "open_amount",
                                   open_amount));
  expect_true("open_amount unchanged when attach_driven off",
              float_near(open_amount.asFloat(), 0.25f));

  expect_true("attach_driven enabled via ClassDB",
              ClassDB::setProperty(paper_mouth, "PaperMouth", "attach_driven",
                                   Variant(true)));
  expect_true("enabling attach_driven syncs occupancy to open_amount",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "open_amount",
                                   open_amount));
  expect_true("open_amount synced from stored occupancy",
              float_near(open_amount.asFloat(), 0.75f));

  paper_mouth->setAttachOccupancy(0.8f);
  expect_true("open_amount get after occupancy while on",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "open_amount",
                                   open_amount));
  expect_true("open_amount filled from occupancy when attach_driven on",
              float_near(open_amount.asFloat(), 0.8f));

  skeleton->resetPoseToRest();
  paper_mouth->apply(*skeleton);
  const Quat jaw_rotation_occupancy =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  expect_true("jaw rotates when attach_driven fills open_amount",
              std::fabs(glm::dot(jaw_rotation_occupancy, glm::identity<Quat>())) <
                  0.999f);

  expect_true("attach_driven disabled via ClassDB",
              ClassDB::setProperty(paper_mouth, "PaperMouth", "attach_driven",
                                   Variant(false)));
  paper_mouth->setAttachOccupancy(0.1f);
  expect_true("open_amount get after occupancy while off again",
              ClassDB::getProperty(paper_mouth, "PaperMouth", "open_amount",
                                   open_amount));
  expect_true("open_amount unchanged when attach_driven off again",
              float_near(open_amount.asFloat(), 0.8f));

  ObjectDB::clear();
  ClassDB::shutdown();
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

Blunder::Object* makePaperMouthPreviewObject(Blunder::Skeleton** out_skeleton) {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  if (object == nullptr) {
    return nullptr;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  AnimationPlayer* player = object->ensureAnimationPlayer();
  AnimationTree* tree = object->ensureAnimationTree();

  const int head = skeleton->addBone("Head", -1);
  const int jaw = skeleton->addBone("Jaw", head);
  (void)jaw;
  skeleton->setBoneRestLocal(static_cast<size_t>(jaw),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.2f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  constexpr const char* kIdleGuid = "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb";
  constexpr const char* kWalkGuid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";

  AnimationClipData idle_clip;
  idle_clip.duration = 1.0f;
  idle_clip.tracks.push_back(makeTranslationTrack(
      "Head", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  player->setClipGuid("idle", kIdleGuid);
  player->injectClipData(kIdleGuid, idle_clip);

  AnimationClipData walk_clip;
  walk_clip.duration = 1.0f;
  walk_clip.tracks.push_back(makeTranslationTrack(
      "Head", AnimationInterpolation::Linear,
      {{0.0f, Vec3(0.0f, 0.0f, 0.0f)}}));
  player->setClipGuid("walk", kWalkGuid);
  player->injectClipData(kWalkGuid, walk_clip);

  tree->addBlendSpacePoint("Locomotion", "idle", 0.0f);
  tree->addBlendSpacePoint("Locomotion", "walk", 1.0f);
  tree->setStateBlendSpace("Locomotion", "Locomotion");

  if (out_skeleton != nullptr) {
    *out_skeleton = skeleton;
  }
  return object;
}

/// Task 2.4: Edit scrub openAmount via AnimationPreviewController without Behaviour Tick.
void test_edit_scrub_paper_mouth_without_behaviour_tick() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* object = makePaperMouthPreviewObject(&skeleton);
  expect_true("preview object", object != nullptr);
  if (object == nullptr) {
    LifecycleDispatch::clear();
    return;
  }

  const int jaw = skeleton->findBoneIndex("Jaw");
  expect_true("jaw bone", jaw >= 0);

  SkeletonPaperMouthModifier* paper_mouth = object->addSkeletonPaperMouthModifier();
  expect_true("paper mouth modifier", paper_mouth != nullptr);
  if (paper_mouth == nullptr) {
    ObjectDB::clear();
    LifecycleDispatch::clear();
    return;
  }

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  expect_true("travel locomotion", controller.travel("Locomotion"));
  expect_true("activate tree", controller.setTreeActive(true));
  controller.setBlendSpaceScalar("Locomotion", 0.0f);

  const size_t paper_mouth_index = 0;
  expect_true("one modifier", controller.skeletonModifierCount() == 1);

  expect_true("scrub closed",
              controller.setSkeletonPaperMouthOpenAmount(paper_mouth_index, 0.0f));
  const Quat jaw_rotation_closed =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;

  expect_true("scrub open",
              controller.setSkeletonPaperMouthOpenAmount(paper_mouth_index, 1.0f));
  const Quat jaw_rotation_open =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  expect_true("open jaw rotation differs from closed via edit scrub",
              std::fabs(glm::dot(jaw_rotation_closed, jaw_rotation_open)) <
                  0.999f);

  expect_true("scrub half open",
              controller.setSkeletonPaperMouthOpenAmount(paper_mouth_index, 0.5f));
  const Quat jaw_rotation_half =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  expect_true("half-open jaw differs from closed",
              std::fabs(glm::dot(jaw_rotation_closed, jaw_rotation_half)) <
                  0.999f);
  expect_true("half-open jaw differs from fully open",
              std::fabs(glm::dot(jaw_rotation_open, jaw_rotation_half)) <
                  0.999f);

  expect_true("no behaviour tick during paper-mouth edit scrub", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

}  // namespace

int main() {
  test_paper_mouth_open_amount_changes_jaw_pose();
  test_paper_mouth_configurable_bone_name();
  test_paper_mouth_attach_driven_open_amount();
  test_edit_scrub_paper_mouth_without_behaviour_tick();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_paper_mouth_test: all passed\n");
  return 0;
}
