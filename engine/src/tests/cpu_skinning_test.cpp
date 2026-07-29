#include "runtime/core/object/skeleton.h"
#include "runtime/function/scene/cpu_skinning.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/mesh_skin_data.h"

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

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b, float eps = 1e-4f) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
         std::fabs(a.z - b.z) < eps;
}

void test_single_bone_rest_pose() {
  using namespace Blunder;

  Skeleton skeleton;
  const int bone = skeleton.addBone("root", -1);
  skeleton.setBoneInverseBind(static_cast<size_t>(bone), Mat4(1.0f));
  skeleton.resetPoseToRest();

  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0};
  skin_data.influences.push_back({});
  skin_data.influences[0].joint_indices = glm::ivec4(0, 0, 0, 0);
  skin_data.influences[0].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

  eastl::vector<MeshVertex> bind_vertices(1);
  bind_vertices[0].position = Vec3(1.0f, 0.0f, 0.0f);
  bind_vertices[0].normal = Vec3(0.0f, 0.0f, 1.0f);

  eastl::vector<MeshVertex> skinned_vertices;
  applyCpuSkinning(skeleton, skin_data, bind_vertices, skinned_vertices);

  expect_true("rest pose keeps bind position",
              vec3_near(skinned_vertices[0].position, Vec3(1.0f, 0.0f, 0.0f)));
}

void test_single_bone_translated_pose() {
  using namespace Blunder;

  Skeleton skeleton;
  const int bone = skeleton.addBone("root", -1);
  skeleton.setBoneInverseBind(static_cast<size_t>(bone), Mat4(1.0f));

  BoneTransform pose;
  pose.translation = Vec3(0.0f, 2.0f, 0.0f);
  skeleton.setBonePoseLocal(static_cast<size_t>(bone), pose);

  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0};
  skin_data.influences.push_back({});
  skin_data.influences[0].joint_indices = glm::ivec4(0, 0, 0, 0);
  skin_data.influences[0].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

  eastl::vector<MeshVertex> bind_vertices(1);
  bind_vertices[0].position = Vec3(1.0f, 0.0f, 0.0f);
  bind_vertices[0].normal = Vec3(0.0f, 0.0f, 1.0f);

  eastl::vector<MeshVertex> skinned_vertices;
  applyCpuSkinning(skeleton, skin_data, bind_vertices, skinned_vertices);

  const Vec3 expected(1.0f, 2.0f, 0.0f);
  expect_true("posed bone translates skinned vertex",
              vec3_near(skinned_vertices[0].position, expected));
}

void test_two_bone_blend() {
  using namespace Blunder;

  Skeleton skeleton;
  const int parent = skeleton.addBone("parent", -1);
  const int child = skeleton.addBone("child", parent);
  skeleton.setBoneInverseBind(static_cast<size_t>(parent), Mat4(1.0f));
  skeleton.setBoneInverseBind(static_cast<size_t>(child), Mat4(1.0f));

  BoneTransform child_pose;
  child_pose.translation = Vec3(0.0f, 4.0f, 0.0f);
  skeleton.setBonePoseLocal(static_cast<size_t>(child), child_pose);

  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0, 1};
  skin_data.influences.push_back({});
  skin_data.influences[0].joint_indices = glm::ivec4(0, 1, 0, 0);
  skin_data.influences[0].weights = glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);

  eastl::vector<MeshVertex> bind_vertices(1);
  bind_vertices[0].position = Vec3(2.0f, 0.0f, 0.0f);

  eastl::vector<MeshVertex> skinned_vertices;
  applyCpuSkinning(skeleton, skin_data, bind_vertices, skinned_vertices);

  // parent global = I, child global translates Y by 4 → blended Y = 2.
  const Vec3 expected(2.0f, 2.0f, 0.0f);
  expect_true("two-bone blend matches hand-computed Y",
              vec3_near(skinned_vertices[0].position, expected));
}

}  // namespace

int main() {
  test_single_bone_rest_pose();
  test_single_bone_translated_pose();
  test_two_bone_blend();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
