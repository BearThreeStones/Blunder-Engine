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

}  // namespace

int main() {
  test_paper_mouth_open_amount_changes_jaw_pose();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("dogwalk_phase6_paper_mouth_test: all passed\n");
  return 0;
}
