#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_attach_modifier.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"

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

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = 1e-4f) {
  return float_near(a.x, b.x, eps) && float_near(a.y, b.y, eps) &&
         float_near(a.z, b.z, eps);
}

/// Authors the Phase 6 chain PaperMouth → Attach → LookAt on a "Dog" entity
/// with a separate "Ball" entity as the Attach child.
void authorSourceScene(Blunder::SceneInstance& source) {
  using namespace Blunder;

  const EntityId dog = source.createEntity("Dog", Vec3(0.0f),
                                           glm::identity<Quat>(), Vec3(1.0f));
  const EntityId ball = source.createEntity("Ball", Vec3(0.0f),
                                            glm::identity<Quat>(), Vec3(1.0f));

  Object* dog_object = source.ensureBoundObject(dog);
  Object* ball_object = source.ensureBoundObject(ball);
  expect_true("dog object", dog_object != nullptr);
  expect_true("ball object", ball_object != nullptr);
  if (dog_object == nullptr || ball_object == nullptr) {
    return;
  }

  Skeleton* skeleton = dog_object->ensureSkeleton();
  skeleton->addBone("Jaw", -1);
  skeleton->addBone("Snout", -1);
  skeleton->addBone("Head", -1);
  skeleton->resetPoseToRest();

  SkeletonPaperMouthModifier* mouth = dog_object->addSkeletonPaperMouthModifier();
  mouth->setBoneName("Jaw");
  mouth->setAttachDriven(true);
  mouth->setOpenAmount(0.42f);

  SkeletonAttachModifier* attach = dog_object->addSkeletonAttachModifier();
  attach->setBoneName("Snout");
  attach->setChildObjectId(ball_object->getId());

  SkeletonLookAtModifier* look_at = dog_object->addSkeletonLookAtModifier();
  look_at->setBoneName("Head");
  look_at->setTarget(Vec3(1.5f, -2.0f, 3.25f));
  look_at->setEnabled(false);
}

/// Task 4.1: modifier type, order and key params survive the scene document.
void test_scene_definition_round_trip() {
  using namespace Blunder;

  ClassDB::initialize();
  ObjectDB::clear();

  SceneInstance source;
  authorSourceScene(source);

  Scene exported;
  expect_true("export", source.exportToScene(exported));

  eastl::string json;
  expect_true("serialize", SceneSerializer::serialize(exported, json, nullptr));

  Scene reloaded;
  expect_true("deserialize",
              SceneSerializer::deserialize(json, reloaded, nullptr));

  const SceneEntityDefinition* dog_def = nullptr;
  for (const SceneEntityDefinition& def : reloaded.getEntities()) {
    if (def.name == "Dog") {
      dog_def = &def;
      break;
    }
  }
  expect_true("dog definition reloaded", dog_def != nullptr);
  if (dog_def == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  expect_true("three modifiers persisted",
              dog_def->skeleton_modifiers.size() == 3);
  if (dog_def->skeleton_modifiers.size() != 3) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  const SceneSkeletonModifierDef& mouth_def = dog_def->skeleton_modifiers[0];
  expect_true("modifier 0 is PaperMouth", mouth_def.type == "PaperMouth");
  expect_true("PaperMouth bone", mouth_def.bone_name == "Jaw");
  expect_true("PaperMouth openAmount", float_near(mouth_def.open_amount, 0.42f));
  expect_true("PaperMouth attachDriven", mouth_def.attach_driven);
  expect_true("PaperMouth enabled", mouth_def.enabled);

  const SceneSkeletonModifierDef& attach_def = dog_def->skeleton_modifiers[1];
  expect_true("modifier 1 is Attach",
              attach_def.type == "SkeletonAttachModifier");
  expect_true("Attach bone", attach_def.bone_name == "Snout");
  expect_true("Attach child by entity name",
              attach_def.child_entity_name == "Ball");

  const SceneSkeletonModifierDef& look_at_def = dog_def->skeleton_modifiers[2];
  expect_true("modifier 2 is LookAt",
              look_at_def.type == "SkeletonLookAtModifier");
  expect_true("LookAt bone", look_at_def.bone_name == "Head");
  expect_true("LookAt target",
              vec3_near(look_at_def.target, Vec3(1.5f, -2.0f, 3.25f)));
  expect_true("LookAt disabled flag persisted", !look_at_def.enabled);

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 4.1: reload restores the live chain without Behaviour assembly.
void test_instantiate_restores_modifier_chain() {
  using namespace Blunder;

  ClassDB::initialize();
  ObjectDB::clear();

  SceneInstance source;
  authorSourceScene(source);

  Scene exported;
  expect_true("export", source.exportToScene(exported));
  eastl::string json;
  expect_true("serialize", SceneSerializer::serialize(exported, json, nullptr));

  source.clear();

  Scene reloaded;
  expect_true("deserialize",
              SceneSerializer::deserialize(json, reloaded, nullptr));

  SceneInstance restored;
  restored.instantiate(reloaded);

  const EntityId dog = restored.findEntityByName("Dog");
  const EntityId ball = restored.findEntityByName("Ball");
  expect_true("dog entity restored", dog != k_invalid_entity_id);
  expect_true("ball entity restored", ball != k_invalid_entity_id);

  Object* dog_object = restored.findBoundObject(dog);
  expect_true("dog object restored", dog_object != nullptr);
  if (dog_object == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  expect_true("chain length restored",
              dog_object->getSkeletonModifierCount() == 3);
  if (dog_object->getSkeletonModifierCount() != 3) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  auto* mouth = static_cast<SkeletonPaperMouthModifier*>(
      dog_object->getSkeletonModifierAt(0));
  auto* attach = static_cast<SkeletonAttachModifier*>(
      dog_object->getSkeletonModifierAt(1));
  auto* look_at = static_cast<SkeletonLookAtModifier*>(
      dog_object->getSkeletonModifierAt(2));

  expect_true("slot 0 type", eastl::string(dog_object->getSkeletonModifierAt(0)
                                               ->getTypeName()) == "PaperMouth");
  expect_true("slot 1 type",
              eastl::string(dog_object->getSkeletonModifierAt(1)->getTypeName()) ==
                  "SkeletonAttachModifier");
  expect_true("slot 2 type",
              eastl::string(dog_object->getSkeletonModifierAt(2)->getTypeName()) ==
                  "SkeletonLookAtModifier");

  expect_true("PaperMouth bone restored", mouth->getBoneName() == "Jaw");
  expect_true("PaperMouth openAmount restored",
              float_near(mouth->getOpenAmount(), 0.42f));
  expect_true("PaperMouth attachDriven restored", mouth->isAttachDriven());
  expect_true("PaperMouth enabled restored", mouth->isEnabled());

  expect_true("Attach bone restored", attach->getBoneName() == "Snout");
  Object* ball_object = restored.findBoundObject(ball);
  expect_true("ball object restored", ball_object != nullptr);
  expect_true("Attach child resolved to reloaded Ball Object",
              ball_object != nullptr &&
                  attach->getChildObjectId() == ball_object->getId());

  expect_true("LookAt bone restored", look_at->getBoneName() == "Head");
  expect_true("LookAt target restored",
              vec3_near(look_at->getTarget(), Vec3(1.5f, -2.0f, 3.25f)));
  expect_true("LookAt disabled restored", !look_at->isEnabled());

  expect_true("skeleton restored", dog_object->hasSkeleton());

  ObjectDB::clear();
  ClassDB::shutdown();
}

/// Task 4.1: entities without modifiers stay clean (no empty JSON key noise).
void test_entity_without_modifiers_omits_key() {
  using namespace Blunder;

  ClassDB::initialize();
  ObjectDB::clear();

  Scene scene;
  SceneEntityDefinition plain;
  plain.name = "Prop";
  scene.getEntities().push_back(plain);

  eastl::string json;
  expect_true("serialize", SceneSerializer::serialize(scene, json, nullptr));
  expect_true("no skeletonModifiers key for plain entity",
              json.find("skeletonModifiers") == eastl::string::npos);

  Scene reloaded;
  expect_true("deserialize",
              SceneSerializer::deserialize(json, reloaded, nullptr));
  expect_true("one entity", reloaded.getEntities().size() == 1);
  if (!reloaded.getEntities().empty()) {
    expect_true("no modifiers parsed",
                reloaded.getEntities()[0].skeleton_modifiers.empty());
  }

  ObjectDB::clear();
  ClassDB::shutdown();
}

}  // namespace

int main() {
  test_scene_definition_round_trip();
  test_instantiate_restores_modifier_chain();
  test_entity_without_modifiers_omits_key();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_modifier_serialize_test: all passed\n");
  return 0;
}
