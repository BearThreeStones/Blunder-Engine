#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
#include "runtime/core/object/skeleton_paper_mouth_modifier.h"
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

}  // namespace

int main() {
  test_paper_mouth_open_amount_changes_jaw_pose();
  test_paper_mouth_configurable_bone_name();
  test_paper_mouth_attach_driven_open_amount();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_paper_mouth_test: all passed\n");
  return 0;
}
