#include "runtime/core/math/math_types.h"
#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_tree.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/lifecycle.h"
#include "runtime/function/editor/animation_preview_controller.h"
#include "runtime/function/editor/document_history.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/editor/inspector_skeleton_modifier_ops.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"

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

Blunder::Object* makePreviewObject(Blunder::Skeleton** out_skeleton) {
  using namespace Blunder;

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
  const int snout = skeleton->addBone("Snout", head);
  (void)jaw;
  (void)snout;
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

}  // namespace

void test_add_skeleton_modifier_command_round_trip() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId entity_id =
      scene.createEntity("Dog", Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f));
  Object* object = scene.ensureBoundObject(entity_id);
  object->ensureSkeleton();
  expect_true("empty chain", object->getSkeletonModifierCount() == 0);

  const size_t index_at_add = 0;
  object->addSkeletonPaperMouthModifier();
  expect_true("one modifier", object->getSkeletonModifierCount() == 1);

  DocumentHistory history;
  history.push(makeAddSkeletonModifierCommand(
      &scene, entity_id, "PaperMouth", index_at_add,
      SelectionSnapshot{entity_id}, SelectionSnapshot{entity_id}));

  expect_true("undo add", history.undo());
  expect_true("removed after undo", object->getSkeletonModifierCount() == 0);
  expect_true("redo add", history.redo());
  expect_true("restored count", object->getSkeletonModifierCount() == 1);
  expect_true("restored type",
              eastl::string(object->getSkeletonModifierAt(0)->getTypeName()) ==
                  "PaperMouth");
}

void test_remove_skeleton_modifier_command_round_trip() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId entity_id =
      scene.createEntity("Dog", Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f));
  Object* object = scene.ensureBoundObject(entity_id);
  object->ensureSkeleton();
  object->addSkeletonPaperMouthModifier();
  object->addSkeletonLookAtModifier();

  SceneSkeletonModifierDef snapshot;
  expect_true("capture mouth", captureSkeletonModifierDef(scene, *object, 0, snapshot));
  expect_true("remove applied", object->removeSkeletonModifierAt(0));
  expect_true("one left", object->getSkeletonModifierCount() == 1);
  expect_true("lookAt remains",
              eastl::string(object->getSkeletonModifierAt(0)->getTypeName()) ==
                  "SkeletonLookAtModifier");

  DocumentHistory history;
  history.push(makeRemoveSkeletonModifierCommand(
      &scene, entity_id, 0, snapshot, SelectionSnapshot{entity_id},
      SelectionSnapshot{entity_id}));

  expect_true("undo remove", history.undo());
  expect_true("two after undo", object->getSkeletonModifierCount() == 2);
  expect_true("mouth restored index 0",
              eastl::string(object->getSkeletonModifierAt(0)->getTypeName()) ==
                  "PaperMouth");
  expect_true("redo remove", history.redo());
  expect_true("one after redo", object->getSkeletonModifierCount() == 1);
}

void test_reorder_skeleton_modifiers_command_round_trip() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId entity_id =
      scene.createEntity("Dog", Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f));
  Object* object = scene.ensureBoundObject(entity_id);
  object->addSkeletonPaperMouthModifier();
  object->addSkeletonLookAtModifier();
  expect_true("reorder move", object->moveSkeletonModifier(0, 2));
  expect_true("lookAt first",
              eastl::string(object->getSkeletonModifierAt(0)->getTypeName()) ==
                  "SkeletonLookAtModifier");

  DocumentHistory history;
  history.push(makeReorderSkeletonModifiersCommand(
      &scene, entity_id, 0, 2, SelectionSnapshot{entity_id},
      SelectionSnapshot{entity_id}));

  expect_true("undo reorder", history.undo());
  expect_true("mouth first",
              eastl::string(object->getSkeletonModifierAt(0)->getTypeName()) ==
                  "PaperMouth");
  expect_true("redo reorder", history.redo());
  expect_true("lookAt front",
              eastl::string(object->getSkeletonModifierAt(0)->getTypeName()) ==
                  "SkeletonLookAtModifier");
}

void test_set_skeleton_modifier_enabled_command_round_trip() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId entity_id =
      scene.createEntity("Dog", Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f));
  Object* object = scene.ensureBoundObject(entity_id);
  object->addSkeletonLookAtModifier();
  object->getSkeletonModifierAt(0)->setEnabled(false);

  DocumentHistory history;
  history.push(makeSetSkeletonModifierEnabledCommand(
      &scene, entity_id, 0, true, false, SelectionSnapshot{entity_id},
      SelectionSnapshot{entity_id}));

  expect_true("disabled", !object->getSkeletonModifierAt(0)->isEnabled());
  expect_true("undo enable", history.undo());
  expect_true("enabled restored", object->getSkeletonModifierAt(0)->isEnabled());
  expect_true("redo disable", history.redo());
  expect_true("disabled again", !object->getSkeletonModifierAt(0)->isEnabled());
}

void test_inspector_rows_and_type_choices() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId dog = scene.createEntity("Dog", Vec3(0.0f), glm::identity<Quat>(),
                                          Vec3(1.0f));
  const EntityId ball = scene.createEntity("Ball", Vec3(0.0f), glm::identity<Quat>(),
                                           Vec3(1.0f));
  Object* dog_object = scene.ensureBoundObject(dog);
  Object* ball_object = scene.ensureBoundObject(ball);
  dog_object->ensureSkeleton();

  SkeletonPaperMouthModifier* mouth = dog_object->addSkeletonPaperMouthModifier();
  mouth->setBoneName("Jaw");
  mouth->setOpenAmount(0.5f);

  SkeletonAttachModifier* attach = dog_object->addSkeletonAttachModifier();
  attach->setBoneName("Snout");
  attach->setChildObjectId(ball_object->getId());

  SkeletonLookAtModifier* look_at = dog_object->addSkeletonLookAtModifier();
  look_at->setBoneName("Head");
  look_at->setTarget(Vec3(1.0f, 2.0f, 3.0f));

  eastl::vector<InspectorSkeletonModifierRowData> rows;
  buildInspectorSkeletonModifierRows(dog_object, &scene, rows);
  expect_true("three rows", rows.size() == 3);
  if (rows.size() == 3) {
    expect_true("row 0 PaperMouth", rows[0].type_name == "PaperMouth");
    expect_true("row 0 jaw", rows[0].bone_name == "Jaw");
    expect_true("row 0 openAmount", float_near(rows[0].open_amount, 0.5f));
    expect_true("row 1 Attach", rows[1].type_name == "SkeletonAttachModifier");
    expect_true("row 1 child", rows[1].child_entity_name == "Ball");
    expect_true("row 2 LookAt", rows[2].type_name == "SkeletonLookAtModifier");
    expect_true("row 2 target", vec3_near(rows[2].target, Vec3(1.0f, 2.0f, 3.0f)));
  }

  eastl::vector<eastl::string> choices;
  buildSkeletonModifierTypeChoices(choices);
  expect_true("three choices", choices.size() == 3);
}

void test_inspector_edits_paper_mouth_preview_after_sample() {
  using namespace Blunder;

  ObjectDB::clear();
  LifecycleDispatch::clear();
  g_tick_calls = 0;
  LifecycleDispatch::setTickHook("Object", on_tick);

  Skeleton* skeleton = nullptr;
  Object* object = makePreviewObject(&skeleton);
  expect_true("preview object", object != nullptr);

  AnimationPreviewController controller;
  controller.bindObject(object, "idle");
  expect_true("add PaperMouth", controller.addSkeletonModifier("PaperMouth"));
  expect_true("travel", controller.travel("Locomotion"));
  expect_true("activate", controller.setTreeActive(true));
  controller.setBlendSpaceScalar("Locomotion", 0.0f);

  const int jaw = skeleton->findBoneIndex("Jaw");
  expect_true("jaw bone", jaw >= 0);

  expect_true("set bone Jaw", controller.setSkeletonPaperMouthBoneName(0, "Jaw"));
  expect_true("scrub closed", controller.setSkeletonPaperMouthOpenAmount(0, 0.0f));
  const Quat jaw_closed =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;

  expect_true("scrub open", controller.setSkeletonPaperMouthOpenAmount(0, 1.0f));
  const Quat jaw_open =
      skeleton->getBonePoseLocal(static_cast<size_t>(jaw)).rotation;
  expect_true("preview reflects openAmount",
              std::fabs(glm::dot(jaw_closed, jaw_open)) < 0.999f);
  expect_true("no behaviour tick", g_tick_calls == 0);

  ObjectDB::clear();
  LifecycleDispatch::clear();
}

void test_inspector_edits_look_at_and_attach_params() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId dog = scene.createEntity("Dog", Vec3(0.0f), glm::identity<Quat>(),
                                          Vec3(1.0f));
  const EntityId ball = scene.createEntity("Ball", Vec3(0.0f), glm::identity<Quat>(),
                                           Vec3(1.0f));
  Object* dog_object = scene.ensureBoundObject(dog);
  Object* ball_object = scene.ensureBoundObject(ball);
  dog_object->ensureSkeleton();
  dog_object->addSkeletonAttachModifier();
  dog_object->addSkeletonLookAtModifier();

  SceneSkeletonModifierDef before_attach;
  expect_true("capture attach", captureSkeletonModifierDef(scene, *dog_object, 0,
                                                           before_attach));
  SceneSkeletonModifierDef after_attach = before_attach;
  after_attach.bone_name = "Snout";
  after_attach.child_entity_name = "Ball";
  applySkeletonModifierFieldsOnObject(&scene, dog_object, 0, after_attach);

  auto* attach = static_cast<SkeletonAttachModifier*>(
      dog_object->getSkeletonModifierAt(0));
  expect_true("attach bone", attach->getBoneName() == "Snout");
  expect_true("attach child", attach->getChildObjectId() == ball_object->getId());

  SceneSkeletonModifierDef before_look;
  expect_true("capture lookAt", captureSkeletonModifierDef(scene, *dog_object, 1,
                                                           before_look));
  SceneSkeletonModifierDef after_look = before_look;
  after_look.bone_name = "Head";
  after_look.target = Vec3(4.0f, 5.0f, 6.0f);
  applySkeletonModifierFieldsOnObject(&scene, dog_object, 1, after_look);

  auto* look_at = static_cast<SkeletonLookAtModifier*>(
      dog_object->getSkeletonModifierAt(1));
  expect_true("lookAt bone", look_at->getBoneName() == "Head");
  expect_true("lookAt target", vec3_near(look_at->getTarget(), Vec3(4.0f, 5.0f, 6.0f)));

  DocumentHistory history;
  history.push(makeSetSkeletonModifierDefCommand(
      &scene, dog, 1, before_look, after_look, SelectionSnapshot{dog},
      SelectionSnapshot{dog}));
  expect_true("undo lookAt edit", history.undo());
  expect_true("lookAt target restored",
              vec3_near(look_at->getTarget(), before_look.target));
  expect_true("redo lookAt edit", history.redo());
  expect_true("lookAt target re-edited",
              vec3_near(look_at->getTarget(), Vec3(4.0f, 5.0f, 6.0f)));
}

void test_inspector_edits_export_to_scene() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId dog = scene.createEntity("Dog", Vec3(0.0f), glm::identity<Quat>(),
                                          Vec3(1.0f));
  Object* object = scene.ensureBoundObject(dog);
  object->ensureSkeleton();
  object->addSkeletonPaperMouthModifier();

  SceneSkeletonModifierDef before;
  expect_true("capture", captureSkeletonModifierDef(scene, *object, 0, before));
  SceneSkeletonModifierDef after = before;
  after.bone_name = "Jaw";
  after.open_amount = 0.75f;
  applySkeletonModifierFieldsOnObject(&scene, object, 0, after);

  Scene exported;
  expect_true("export", scene.exportToScene(exported));
  expect_true("one entity", exported.getEntities().size() == 1);
  if (exported.getEntities().size() == 1) {
    const SceneEntityDefinition& def = exported.getEntities()[0];
    expect_true("one modifier", def.skeleton_modifiers.size() == 1);
    if (def.skeleton_modifiers.size() == 1) {
      expect_true("exported bone", def.skeleton_modifiers[0].bone_name == "Jaw");
      expect_true("exported openAmount",
                  float_near(def.skeleton_modifiers[0].open_amount, 0.75f));
    }
  }
}

int main() {
  Blunder::ClassDB::initialize();

  test_add_skeleton_modifier_command_round_trip();
  test_remove_skeleton_modifier_command_round_trip();
  test_reorder_skeleton_modifiers_command_round_trip();
  test_set_skeleton_modifier_enabled_command_round_trip();
  test_inspector_rows_and_type_choices();
  test_inspector_edits_paper_mouth_preview_after_sample();
  test_inspector_edits_look_at_and_attach_params();
  test_inspector_edits_export_to_scene();

  Blunder::ClassDB::shutdown();

  const int exit_code = g_failures != 0 ? 1 : 0;
  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
  } else {
    std::fprintf(stderr,
                 "inspector_skeleton_modifier_commands_test: all passed\n");
  }
  return exit_code;
}
