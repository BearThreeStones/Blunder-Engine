#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
#include "runtime/core/object/skeleton_modifier_catalog.h"
#include "runtime/core/object/skeleton_spring_bone_modifier.h"
#include "runtime/core/reflection/class_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_serializer.h"
#include "runtime/function/script/animation_frame.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 1.0e-4f) {
  return std::fabs(a - b) < eps;
}

void add_forward_chain(Blunder::Skeleton& skeleton) {
  using namespace Blunder;
  const int root = skeleton.addBone("Logo", -1);
  const int tip = skeleton.addBone("LogoTip", root);
  BoneTransform rest;
  rest.translation = Vec3(0.0f, 1.0f, 0.0f);
  skeleton.setBoneRestLocal(static_cast<size_t>(tip), rest);
  skeleton.resetPoseToRest();
}

void add_twisted_chain(Blunder::Skeleton& skeleton) {
  using namespace Blunder;
  const int root = skeleton.addBone("Logo", -1);
  const int tip = skeleton.addBone("LogoTip", root);
  BoneTransform logo_rest;
  logo_rest.rotation =
      glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 1.0f, 0.0f));
  skeleton.setBoneRestLocal(static_cast<size_t>(root), logo_rest);
  BoneTransform tip_rest;
  tip_rest.translation = Vec3(0.0f, 1.0f, 0.0f);
  skeleton.setBoneRestLocal(static_cast<size_t>(tip), tip_rest);
  skeleton.resetPoseToRest();
}

Blunder::Mat4 host_matrix(const Blunder::Vec3& position, float scale) {
  using namespace Blunder;
  return glm::translate(Mat4(1.0f), position) *
         glm::scale(Mat4(1.0f), Vec3(scale));
}

Blunder::Vec3 logo_local_axis(const Blunder::Skeleton& skeleton,
                              const Blunder::Vec3& axis) {
  using namespace Blunder;
  const int logo = skeleton.findBoneIndex("Logo");
  if (logo < 0) {
    return Vec3(0.0f);
  }
  return skeleton.getBonePoseLocal(static_cast<size_t>(logo)).rotation * axis;
}

float tip_z(const Blunder::Skeleton& skeleton) {
  const int tip = skeleton.findBoneIndex("LogoTip");
  if (tip < 0) {
    return 0.0f;
  }
  const Blunder::Mat4 global =
      skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(tip));
  return global[3].z;
}

void step_spring(Blunder::SkeletonSpringBoneModifier& spring,
                 Blunder::Skeleton& skeleton, float dt, int steps) {
  spring.setDeltaTime(dt);
  for (int i = 0; i < steps; ++i) {
    spring.apply(skeleton);
  }
}

void test_catalog_and_classdb() {
  using namespace Blunder;
  ClassDB::initialize();

  expect_true("SpringBone registered", ClassDB::hasClass("SpringBone"));
  eastl::unique_ptr<SkeletonModifier> created =
      SkeletonModifierCatalog::construct("SpringBone");
  expect_true("construct SpringBone", created != nullptr);
  expect_true("type name",
              created != nullptr &&
                  std::strcmp(created->getTypeName(), "SpringBone") == 0);

  eastl::vector<eastl::string> names;
  SkeletonModifierCatalog::listAddMenuTypes(names);
  bool listed = false;
  for (const eastl::string& name : names) {
    if (name == "SpringBone") {
      listed = true;
    }
  }
  expect_true("Add menu lists SpringBone", listed);
  expect_true("four product types", names.size() == 4);

  auto* spring = static_cast<SkeletonSpringBoneModifier*>(created.get());
  expect_true("set stiffness",
              ClassDB::setProperty(spring, "SpringBone", "stiffness",
                                   Variant(0.55f)));
  Variant stiffness;
  expect_true("get stiffness",
              ClassDB::getProperty(spring, "SpringBone", "stiffness", stiffness));
  expect_true("stiffness value", float_near(stiffness.asFloat(), 0.55f));
  expect_true("stiffness clamps",
              ClassDB::setProperty(spring, "SpringBone", "stiffness",
                                   Variant(4.0f)));
  expect_true("get clamped stiffness",
              ClassDB::getProperty(spring, "SpringBone", "stiffness", stiffness));
  expect_true("clamped stiffness is 1", float_near(stiffness.asFloat(), 1.0f));

  ClassDB::shutdown();
}

void test_gravity_sags_and_stiffness_holds() {
  using namespace Blunder;
  Skeleton skeleton;
  add_forward_chain(skeleton);

  SkeletonSpringBoneModifier loose;
  loose.setRootBoneName("Logo");
  loose.setEndBoneName("LogoTip");
  loose.setStiffness(0.0f);
  loose.setDrag(0.0f);
  loose.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  loose.setDeltaTime(1.0f / 30.0f);
  loose.apply(skeleton);
  expect_true("first step does not pop", std::fabs(tip_z(skeleton)) < 1.0e-4f);

  step_spring(loose, skeleton, 1.0f / 30.0f, 24);
  const float sagged = tip_z(skeleton);
  expect_true("gravity sags the chain", sagged < -0.02f);

  Skeleton held;
  add_forward_chain(held);
  SkeletonSpringBoneModifier stiff;
  stiff.setRootBoneName("Logo");
  stiff.setEndBoneName("LogoTip");
  stiff.setStiffness(1.0f);
  stiff.setDrag(0.0f);
  stiff.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  step_spring(stiff, held, 1.0f / 30.0f, 24);
  expect_true("full stiffness holds the animated pose",
              std::fabs(tip_z(held)) < 1.0e-3f);
}

void test_stiffness_holds_twist() {
  using namespace Blunder;
  Skeleton skeleton;
  add_twisted_chain(skeleton);
  const int logo = skeleton.findBoneIndex("Logo");
  expect_true("twisted logo bone", logo >= 0);
  if (logo < 0) {
    return;
  }
  const Vec3 rest_side = logo_local_axis(skeleton, Vec3(1.0f, 0.0f, 0.0f));
  const Vec3 rest_aim = logo_local_axis(skeleton, Vec3(0.0f, 1.0f, 0.0f));

  SkeletonSpringBoneModifier stiff;
  stiff.setRootBoneName("Logo");
  stiff.setEndBoneName("LogoTip");
  stiff.setStiffness(1.0f);
  stiff.setDrag(0.0f);
  stiff.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  stiff.setDeltaTime(1.0f / 30.0f);
  stiff.apply(skeleton);
  expect_true("seed apply keeps rest roll",
              glm::dot(logo_local_axis(skeleton, Vec3(1.0f, 0.0f, 0.0f)),
                       rest_side) > 0.999f);

  step_spring(stiff, skeleton, 1.0f / 30.0f, 8);
  const Vec3 pose_side = logo_local_axis(skeleton, Vec3(1.0f, 0.0f, 0.0f));
  const Vec3 pose_aim = logo_local_axis(skeleton, Vec3(0.0f, 1.0f, 0.0f));
  expect_true("stiffness 1 keeps rest roll around child offset",
              glm::dot(pose_side, rest_side) > 0.999f);
  expect_true("stiffness 1 still aims along child offset",
              glm::dot(pose_aim, rest_aim) > 0.999f);
  expect_true("twisted tip stays at rest", std::fabs(tip_z(skeleton)) < 1.0e-3f);
}

void test_scaled_host_length_and_lag() {
  using namespace Blunder;
  const Vec3 axis(0.0f, 1.0f, 0.0f);
  const float model_length = 1.0f;
  const float scale = 2.0f;
  const Vec3 move(10.0f, 0.0f, 0.0f);
  const float dt = 1.0f / 30.0f;

  auto lag_aim = [&](float host_scale) {
    Skeleton skeleton;
    add_forward_chain(skeleton);
    SkeletonSpringBoneModifier spring;
    spring.setRootBoneName("Logo");
    spring.setEndBoneName("LogoTip");
    spring.setStiffness(0.0f);
    spring.setDrag(1.0f);
    spring.setGravity(Vec3(0.0f));
    spring.setDeltaTime(dt);
    spring.setHostWorldMatrix(host_matrix(Vec3(0.0f), host_scale));
    spring.apply(skeleton);
    spring.setHostWorldMatrix(host_matrix(move, host_scale));
    spring.apply(skeleton);
    return glm::normalize(logo_local_axis(skeleton, axis));
  };

  const Vec3 scaled_aim = lag_aim(scale);
  const Vec3 unit_aim = lag_aim(1.0f);

  const Vec3 seed_tail(0.0f, scale * model_length, 0.0f);
  const Vec3 head_after = move;
  const Vec3 expected_world =
      head_after + glm::normalize(seed_tail - head_after) *
                       (scale * model_length);
  const Vec3 expected_model = Vec3(
      glm::inverse(host_matrix(move, scale)) * Vec4(expected_world, 1.0f));
  const Vec3 expected_dir = glm::normalize(expected_model);

  expect_true("scaled host lag matches world rest length",
              glm::dot(scaled_aim, expected_dir) > 0.999f);
  expect_true("scale 2 lag is not the unscaled model-length lag",
              glm::dot(scaled_aim, unit_aim) < 0.999f);

  Skeleton held;
  add_forward_chain(held);
  SkeletonSpringBoneModifier stiff;
  stiff.setRootBoneName("Logo");
  stiff.setEndBoneName("LogoTip");
  stiff.setStiffness(1.0f);
  stiff.setDrag(0.0f);
  stiff.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  stiff.setHostWorldMatrix(host_matrix(Vec3(0.0f), scale));
  step_spring(stiff, held, dt, 8);
  expect_true("stiffness 1 at scale 2 still holds aim",
              glm::dot(glm::normalize(logo_local_axis(held, axis)), axis) >
                  0.999f);
}

void test_drag_and_inertia() {
  using namespace Blunder;
  Skeleton loose_skeleton;
  add_forward_chain(loose_skeleton);
  SkeletonSpringBoneModifier loose;
  loose.setRootBoneName("Logo");
  loose.setEndBoneName("LogoTip");
  loose.setStiffness(0.0f);
  loose.setDrag(0.0f);
  loose.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  step_spring(loose, loose_skeleton, 1.0f / 30.0f, 18);
  const float loose_z = tip_z(loose_skeleton);

  Skeleton damped_skeleton;
  add_forward_chain(damped_skeleton);
  SkeletonSpringBoneModifier damped;
  damped.setRootBoneName("Logo");
  damped.setEndBoneName("LogoTip");
  damped.setStiffness(0.0f);
  damped.setDrag(1.0f);
  damped.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  step_spring(damped, damped_skeleton, 1.0f / 30.0f, 18);
  const float damped_z = tip_z(damped_skeleton);
  expect_true("undamped chain sags farther than full drag", loose_z < damped_z);
  expect_true("full drag still responds to gravity", damped_z < -1.0e-4f);

  // Sample while the chain is still falling. A longer run reaches the
  // hanging pose, where further steps no longer decrease Z.
  Skeleton coast_skeleton;
  add_forward_chain(coast_skeleton);
  SkeletonSpringBoneModifier coast;
  coast.setRootBoneName("Logo");
  coast.setEndBoneName("LogoTip");
  coast.setStiffness(0.0f);
  coast.setDrag(0.0f);
  coast.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  step_spring(coast, coast_skeleton, 1.0f / 30.0f, 6);
  const float before_coast = tip_z(coast_skeleton);
  coast.setGravity(Vec3(0.0f));
  step_spring(coast, coast_skeleton, 1.0f / 30.0f, 2);
  expect_true("inertia keeps moving after gravity clears",
              tip_z(coast_skeleton) < before_coast - 1.0e-4f);
}

void test_disabled_and_missing_bone() {
  using namespace Blunder;
  Skeleton skeleton;
  add_forward_chain(skeleton);
  const float before = tip_z(skeleton);

  SkeletonSpringBoneModifier spring;
  spring.setRootBoneName("Logo");
  spring.setEndBoneName("LogoTip");
  spring.setStiffness(0.0f);
  spring.setDrag(0.0f);
  spring.setGravity(Vec3(0.0f, 0.0f, -30.0f));
  spring.setEnabled(false);
  step_spring(spring, skeleton, 1.0f / 30.0f, 10);
  expect_true("disabled spring leaves the pose", float_near(tip_z(skeleton), before));

  spring.setEnabled(true);
  spring.setRootBoneName("Missing");
  step_spring(spring, skeleton, 1.0f / 30.0f, 10);
  expect_true("missing root is a no-op", float_near(tip_z(skeleton), before));
}

void test_scene_round_trip() {
  using namespace Blunder;
  ClassDB::initialize();
  ObjectDB::clear();

  SceneInstance source;
  const EntityId logo = source.createEntity("Logo", Vec3(0.0f),
                                            glm::identity<Quat>(), Vec3(1.0f));
  Object* object = source.ensureBoundObject(logo);
  expect_true("logo object", object != nullptr);
  if (object == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }
  Skeleton* skeleton = object->ensureSkeleton();
  skeleton->addBone("Logo", -1);
  SkeletonSpringBoneModifier* spring = object->addSkeletonSpringBoneModifier();
  expect_true("spring added", spring != nullptr);
  if (spring == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }
  spring->setRootBoneName("Logo");
  spring->setEndBoneName("LogoTip");
  spring->setStiffness(0.35f);
  spring->setDrag(0.15f);
  spring->setGravity(Vec3(0.0f, 1.0f, -9.8f));
  spring->setEndBoneLength(0.25f);
  spring->setEnabled(false);

  Scene exported;
  expect_true("export", source.exportToScene(exported));
  eastl::string json;
  expect_true("serialize", SceneSerializer::serialize(exported, json, nullptr));
  expect_true("json type", json.find("\"SpringBone\"") != eastl::string::npos);
  expect_true("json end bone", json.find("LogoTip") != eastl::string::npos);

  Scene reloaded;
  expect_true("deserialize",
              SceneSerializer::deserialize(json, reloaded, nullptr));
  expect_true("one entity", reloaded.getEntities().size() == 1);
  if (reloaded.getEntities().empty() ||
      reloaded.getEntities()[0].skeleton_modifiers.empty()) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }
  const SceneSkeletonModifierDef& def =
      reloaded.getEntities()[0].skeleton_modifiers[0];
  expect_true("def type", def.type == "SpringBone");
  expect_true("def disabled", !def.enabled);
  expect_true("def root", def.bone_name == "Logo");
  expect_true("def end", def.end_bone_name == "LogoTip");
  expect_true("def stiffness", float_near(def.stiffness, 0.35f));
  expect_true("def drag", float_near(def.drag, 0.15f));
  expect_true("def gravity z", float_near(def.gravity.z, -9.8f));
  expect_true("def end length", float_near(def.end_bone_length, 0.25f));

  SceneInstance instance;
  instance.instantiate(reloaded);
  Object* loaded = instance.findBoundObject(instance.findEntityByName("Logo"));
  expect_true("loaded object", loaded != nullptr);
  if (loaded == nullptr || loaded->getSkeletonModifierCount() != 1) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }
  auto* loaded_spring = static_cast<SkeletonSpringBoneModifier*>(
      loaded->getSkeletonModifierAt(0));
  expect_true("loaded type",
              std::strcmp(loaded_spring->getTypeName(), "SpringBone") == 0);
  expect_true("loaded stiffness", float_near(loaded_spring->getStiffness(), 0.35f));
  expect_true("loaded end", loaded_spring->getEndBoneName() == "LogoTip");
  expect_true("loaded disabled", !loaded_spring->isEnabled());

  ObjectDB::clear();
  ClassDB::shutdown();
}

void test_c_abi() {
  using namespace Blunder;
  ObjectDB::clear();
  const ObjectId host_id = ObjectDB::create();
  Object* host = ObjectDB::get(host_id);
  expect_true("host", host != nullptr);
  if (host == nullptr) {
    return;
  }
  SkeletonLookAtModifier* look_at = host->addSkeletonLookAtModifier();
  SkeletonSpringBoneModifier* spring = host->addSkeletonSpringBoneModifier();
  expect_true("look at", look_at != nullptr);
  expect_true("spring", spring != nullptr);
  const BlunderObjectId id = static_cast<BlunderObjectId>(host_id);

  expect_true("set root",
              blunder_skeleton_modifier_set_spring_bone_root_bone_name(
                  id, 1, "Logo") == BLUNDER_ENGINE_OK);
  char name[64] = {};
  expect_true("get root",
              blunder_skeleton_modifier_get_spring_bone_root_bone_name(
                  id, 1, name, static_cast<int>(sizeof(name))) ==
                  BLUNDER_ENGINE_OK);
  expect_true("root value", std::strcmp(name, "Logo") == 0);

  expect_true("set end",
              blunder_skeleton_modifier_set_spring_bone_end_bone_name(
                  id, 1, "LogoTip") == BLUNDER_ENGINE_OK);
  name[0] = '\0';
  expect_true("get end",
              blunder_skeleton_modifier_get_spring_bone_end_bone_name(
                  id, 1, name, static_cast<int>(sizeof(name))) ==
                  BLUNDER_ENGINE_OK);
  expect_true("end value", std::strcmp(name, "LogoTip") == 0);

  expect_true("set stiffness",
              blunder_skeleton_modifier_set_spring_bone_stiffness(id, 1, 0.4f) ==
                  BLUNDER_ENGINE_OK);
  float stiffness = 0.0f;
  expect_true("get stiffness",
              blunder_skeleton_modifier_get_spring_bone_stiffness(
                  id, 1, &stiffness) == BLUNDER_ENGINE_OK);
  expect_true("stiffness value", float_near(stiffness, 0.4f));

  expect_true("set drag",
              blunder_skeleton_modifier_set_spring_bone_drag(id, 1, 0.6f) ==
                  BLUNDER_ENGINE_OK);
  float drag = 0.0f;
  expect_true("get drag", blunder_skeleton_modifier_get_spring_bone_drag(
                              id, 1, &drag) == BLUNDER_ENGINE_OK);
  expect_true("drag value", float_near(drag, 0.6f));

  expect_true("set gravity",
              blunder_skeleton_modifier_set_spring_bone_gravity(
                  id, 1, 0.0f, 2.0f, -3.0f) == BLUNDER_ENGINE_OK);
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  expect_true("get gravity",
              blunder_skeleton_modifier_get_spring_bone_gravity(
                  id, 1, &gx, &gy, &gz) == BLUNDER_ENGINE_OK);
  expect_true("gravity value",
              float_near(gx, 0.0f) && float_near(gy, 2.0f) &&
                  float_near(gz, -3.0f));

  expect_true("set end length",
              blunder_skeleton_modifier_set_spring_bone_end_length(id, 1, 0.5f) ==
                  BLUNDER_ENGINE_OK);
  float end_length = 0.0f;
  expect_true("get end length",
              blunder_skeleton_modifier_get_spring_bone_end_length(
                  id, 1, &end_length) == BLUNDER_ENGINE_OK);
  expect_true("end length value", float_near(end_length, 0.5f));

  expect_true("wrong type",
              blunder_skeleton_modifier_set_spring_bone_stiffness(id, 0, 0.2f) ==
                  BLUNDER_ENGINE_ERROR);
  expect_true("abi version is 15", BLUNDER_ENGINE_C_ABI_VERSION == 15);

  BlunderNativeAbi abi{};
  blunder_native_abi_fill_from_process(&abi);
  expect_true("table set stiffness",
              abi.skeleton_modifier_set_spring_bone_stiffness != nullptr);
  expect_true("table get gravity",
              abi.skeleton_modifier_get_spring_bone_gravity != nullptr);

  ObjectDB::clear();
}

void test_play_frame_integrates() {
  using namespace Blunder;
  ObjectDB::clear();

  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object", object != nullptr);
  if (object == nullptr) {
    return;
  }
  Skeleton* skeleton = object->ensureSkeleton();
  add_forward_chain(*skeleton);
  AnimationPlayer* player = object->ensureAnimationPlayer();
  SkeletonSpringBoneModifier* spring = object->addSkeletonSpringBoneModifier();
  expect_true("player", player != nullptr);
  expect_true("spring", spring != nullptr);
  if (player == nullptr || spring == nullptr) {
    ObjectDB::clear();
    return;
  }
  spring->setRootBoneName("Logo");
  spring->setEndBoneName("LogoTip");
  spring->setStiffness(0.0f);
  spring->setDrag(0.0f);
  spring->setGravity(Vec3(0.0f, 0.0f, -30.0f));

  const eastl::string guid = "dddddddd-dddd-dddd-dddd-dddddddddddd";
  AnimationClipData clip;
  clip.duration = 10.0f;
  player->setClipGuid("idle", guid);
  player->injectClipData(guid, clip);
  player->setLoop(true);
  expect_true("play", player->play("idle"));
  expect_true("play seeds without a pop", std::fabs(tip_z(*skeleton)) < 1.0e-3f);

  for (int i = 0; i < 30; ++i) {
    tickObjectAnimationPlayFrame(object, 1.0f / 30.0f, /*play_paused=*/false);
  }
  expect_true("play frames sag the logo chain", tip_z(*skeleton) < -0.02f);

  ObjectDB::clear();
}

}  // namespace

int main() {
  test_catalog_and_classdb();
  test_gravity_sags_and_stiffness_holds();
  test_stiffness_holds_twist();
  test_scaled_host_length_and_lag();
  test_drag_and_inertia();
  test_disabled_and_missing_bone();
  test_scene_round_trip();
  test_c_abi();
  test_play_frame_integrates();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("spring_bone_modifier_test: all passed\n");
  return 0;
}
