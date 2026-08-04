#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_look_at_modifier.h"
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

bool float_near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

float aim_dot_for_bone(const Blunder::Skeleton& skeleton, size_t bone_index,
                       const Blunder::Vec3& target) {
  const Blunder::Mat4 head_global =
      skeleton.getBoneGlobalPoseMatrix(bone_index);
  const Blunder::Vec3 bone_pos(head_global[3]);
  const Blunder::Vec3 bone_forward = glm::normalize(
      Blunder::Vec3(head_global * Blunder::Vec4(0.0f, 1.0f, 0.0f, 0.0f)));
  const Blunder::Vec3 desired_forward = glm::normalize(target - bone_pos);
  return glm::dot(bone_forward, desired_forward);
}

bool set_look_at_target_via_classdb(Blunder::SkeletonLookAtModifier* modifier,
                                    const Blunder::Vec3& target) {
  using namespace Blunder;
  return ClassDB::setProperty(modifier, "SkeletonLookAtModifier", "target_x",
                              Variant(target.x)) &&
         ClassDB::setProperty(modifier, "SkeletonLookAtModifier", "target_y",
                              Variant(target.y)) &&
         ClassDB::setProperty(modifier, "SkeletonLookAtModifier", "target_z",
                              Variant(target.z));
}

bool get_look_at_target_via_classdb(const Blunder::SkeletonLookAtModifier* modifier,
                                    Blunder::Vec3& out_target) {
  using namespace Blunder;
  Variant x;
  Variant y;
  Variant z;
  if (!ClassDB::getProperty(modifier, "SkeletonLookAtModifier", "target_x", x) ||
      !ClassDB::getProperty(modifier, "SkeletonLookAtModifier", "target_y", y) ||
      !ClassDB::getProperty(modifier, "SkeletonLookAtModifier", "target_z", z)) {
    return false;
  }
  out_target = Vec3(x.asFloat(), y.asFloat(), z.asFloat());
  return true;
}

/// Task 1.1: LookAt ClassDB product — bone name and target independently
/// configurable; apply reads the configured min field set.
void test_look_at_product_configurable_bone_and_target() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("SkeletonLookAtModifier registered",
              ClassDB::hasClass("SkeletonLookAtModifier"));

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    ClassDB::shutdown();
    return;
  }

  Skeleton* skeleton = object->ensureSkeleton();
  const int hips = skeleton->addBone("Hips", -1);
  const int neck = skeleton->addBone("Neck", hips);
  const int head = skeleton->addBone("Head", neck);
  skeleton->setBoneRestLocal(static_cast<size_t>(neck),
                             BoneTransform{Vec3(0.0f, 0.0f, 0.5f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->setBoneRestLocal(static_cast<size_t>(head),
                             BoneTransform{Vec3(0.0f, 0.0f, 1.0f),
                                           glm::identity<Quat>(), Vec3(1.0f)});
  skeleton->resetPoseToRest();

  SkeletonLookAtModifier* look_at = object->addSkeletonLookAtModifier();
  expect_true("look-at product created", look_at != nullptr);
  if (look_at == nullptr) {
    ObjectDB::clear();
    ClassDB::shutdown();
    return;
  }

  expect_true("bone_name set via ClassDB",
              ClassDB::setProperty(look_at, "SkeletonLookAtModifier", "bone_name",
                                   Variant(eastl::string("Head"))));
  const Vec3 target_a(1.0f, 1.0f, 1.0f);
  expect_true("target set via ClassDB", set_look_at_target_via_classdb(look_at, target_a));

  Variant bone_name;
  expect_true("bone_name get via ClassDB",
              ClassDB::getProperty(look_at, "SkeletonLookAtModifier", "bone_name",
                                   bone_name));
  expect_true("bone_name round-trip", bone_name.asString() == "Head");

  Vec3 target_round_trip;
  expect_true("target get via ClassDB",
              get_look_at_target_via_classdb(look_at, target_round_trip));
  expect_true("target round-trip",
              float_near(target_round_trip.x, target_a.x) &&
                  float_near(target_round_trip.y, target_a.y) &&
                  float_near(target_round_trip.z, target_a.z));

  skeleton->resetPoseToRest();
  look_at->apply(*skeleton);
  const float aim_a =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_a);
  const Quat rotation_target_a =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("head aims at target A", aim_a > 0.95f);

  const Vec3 target_b(-2.0f, 0.5f, 1.0f);
  expect_true("target B set via ClassDB",
              set_look_at_target_via_classdb(look_at, target_b));
  skeleton->resetPoseToRest();
  look_at->apply(*skeleton);
  const float aim_b =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(head), target_b);
  const Quat rotation_target_b =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  expect_true("head aims at target B", aim_b > 0.95f);
  expect_true("target change rotates bone",
              std::fabs(glm::dot(rotation_target_a, rotation_target_b)) <
                  0.999f);

  expect_true("bone_name switched to Neck via ClassDB",
              ClassDB::setProperty(look_at, "SkeletonLookAtModifier", "bone_name",
                                   Variant(eastl::string("Neck"))));
  skeleton->resetPoseToRest();
  const Quat head_rotation_before =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  look_at->apply(*skeleton);
  const Quat head_rotation_after =
      skeleton->getBonePoseLocal(static_cast<size_t>(head)).rotation;
  const float neck_aim =
      aim_dot_for_bone(*skeleton, static_cast<size_t>(neck), target_b);
  expect_true("neck aims after bone change", neck_aim > 0.95f);
  expect_true("head rotation unchanged when aiming neck",
              std::fabs(glm::dot(head_rotation_before, head_rotation_after)) >
                  0.999f);

  ObjectDB::clear();
  ClassDB::shutdown();
}

}  // namespace

int main() {
  test_look_at_product_configurable_bone_and_target();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_look_at_product_test: all passed\n");
  return 0;
}
