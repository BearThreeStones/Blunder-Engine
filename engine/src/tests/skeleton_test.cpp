#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"
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

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = 1e-4f) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
         std::fabs(a.z - b.z) < eps;
}

void test_skeleton_hierarchy_rest_pose_reset() {
  using namespace Blunder;

  Skeleton skeleton;
  const int root = skeleton.addBone("root", -1);
  const int arm = skeleton.addBone("arm", root);
  expect_true("root index", root == 0);
  expect_true("arm index", arm == 1);
  expect_true("bone count", skeleton.getBoneCount() == 2);
  expect_true("find root", skeleton.findBoneIndex("root") == 0);
  expect_true("find arm", skeleton.findBoneIndex("arm") == 1);
  expect_true("parent", skeleton.getParentIndex(arm) == root);

  BoneTransform root_rest;
  root_rest.translation = Vec3(1.0f, 0.0f, 0.0f);
  skeleton.setBoneRestLocal(static_cast<size_t>(root), root_rest);

  BoneTransform arm_rest;
  arm_rest.translation = Vec3(0.0f, 2.0f, 0.0f);
  skeleton.setBoneRestLocal(static_cast<size_t>(arm), arm_rest);
  skeleton.resetPoseToRest();

  BoneTransform arm_pose;
  arm_pose.translation = Vec3(0.0f, 5.0f, 0.0f);
  skeleton.setBonePoseLocal(static_cast<size_t>(arm), arm_pose);
  expect_true("pose differs from rest",
              !vec3_near(skeleton.getBonePoseLocal(static_cast<size_t>(arm))
                             .translation,
                         arm_rest.translation));

  const Mat4 arm_global_pose =
      skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(arm));
  const Vec3 arm_world = Vec3(arm_global_pose[3]);
  expect_true("global pose accumulates parent",
              vec3_near(arm_world, Vec3(1.0f, 5.0f, 0.0f)));

  skeleton.resetPoseToRest();
  expect_true("reset restores local pose",
              vec3_near(skeleton.getBonePoseLocal(static_cast<size_t>(arm))
                            .translation,
                        arm_rest.translation));
}

void test_inverse_bind_matrix() {
  using namespace Blunder;

  Skeleton skeleton;
  const int bone = skeleton.addBone("joint", -1);
  Mat4 bind = glm::translate(Mat4(1.0f), Vec3(0.0f, 0.0f, 3.0f));
  skeleton.setBoneInverseBind(static_cast<size_t>(bone), bind);
  expect_true("inverse bind round-trip",
              skeleton.getBoneInverseBind(static_cast<size_t>(bone)) == bind);
}

void test_object_hosts_skeleton() {
  using namespace Blunder;

  ObjectDB::clear();
  const ObjectId id = ObjectDB::create();
  Object* object = ObjectDB::get(id);
  expect_true("object created", object != nullptr);
  if (object == nullptr) {
    return;
  }

  expect_true("no skeleton initially", !object->hasSkeleton());
  expect_true("get skeleton null", object->getSkeleton() == nullptr);

  Skeleton* skeleton = object->ensureSkeleton();
  expect_true("ensure returns skeleton", skeleton != nullptr);
  expect_true("has skeleton", object->hasSkeleton());
  expect_true("get matches ensure", object->getSkeleton() == skeleton);

  Skeleton* again = object->ensureSkeleton();
  expect_true("ensure idempotent", again == skeleton);

  skeleton->addBone("hip", -1);
  expect_true("bone on hosted skeleton", skeleton->getBoneCount() == 1);

  object->clearSkeleton();
  expect_true("cleared", !object->hasSkeleton());
  expect_true("get null after clear", object->getSkeleton() == nullptr);

  ObjectDB::clear();
}

void test_classdb_skeleton_registration() {
  using namespace Blunder;

  ClassDB::initialize();
  expect_true("Skeleton class registered", ClassDB::hasClass("Skeleton"));

  Skeleton skeleton;
  skeleton.addBone("a", -1);
  skeleton.addBone("b", 0);

  Variant count;
  expect_true("bone_count property get",
              ClassDB::getProperty(&skeleton, "Skeleton", "bone_count", count));
  expect_true("bone_count value", count.asInt() == 2);

  ClassDB::shutdown();
}

}  // namespace

int main() {
  test_skeleton_hierarchy_rest_pose_reset();
  test_inverse_bind_matrix();
  test_object_hosts_skeleton();
  test_classdb_skeleton_registration();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
