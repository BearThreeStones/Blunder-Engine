#include "runtime/core/object/skeleton.h"
#include "runtime/function/scene/cpu_skinning.h"
#include "runtime/function/scene/gpu_skinning.h"
#include "runtime/resource/asset/mesh_skin_data.h"

#include <cmath>
#include <cstdio>

namespace {

// Task 4.4 tolerance: absolute per-component on deformed positions (metres).
constexpr float k_pose_parity_abs_eps = 1e-3f;

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool vec3_near(const Blunder::Vec3& a, const Blunder::Vec3& b,
               float eps = k_pose_parity_abs_eps) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
         std::fabs(a.z - b.z) < eps;
}

bool all_positions_near(const eastl::vector<Blunder::MeshVertex>& cpu,
                        const eastl::vector<Blunder::Vec3>& gpu_ref) {
  if (cpu.size() != gpu_ref.size()) {
    return false;
  }
  for (size_t i = 0; i < cpu.size(); ++i) {
    if (!vec3_near(cpu[i].position, gpu_ref[i])) {
      return false;
    }
  }
  return true;
}

void build_two_bone_pose_fixture(Blunder::Skeleton& skeleton,
                                 Blunder::MeshSkinData& skin_data,
                                 eastl::vector<Blunder::MeshVertex>& bind_vertices) {
  using namespace Blunder;

  const int parent = skeleton.addBone("hip", -1);
  const int child = skeleton.addBone("knee", parent);

  Mat4 parent_inverse_bind(1.0f);
  parent_inverse_bind[3] = Vec4(-1.0f, 0.0f, 0.0f, 1.0f);
  skeleton.setBoneInverseBind(static_cast<size_t>(parent), parent_inverse_bind);

  Mat4 child_inverse_bind(1.0f);
  child_inverse_bind[3] = Vec4(-2.0f, 0.0f, 0.0f, 1.0f);
  skeleton.setBoneInverseBind(static_cast<size_t>(child), child_inverse_bind);

  BoneTransform parent_pose;
  parent_pose.rotation =
      glm::angleAxis(glm::radians(30.0f), Vec3(0.0f, 1.0f, 0.0f));
  skeleton.setBonePoseLocal(static_cast<size_t>(parent), parent_pose);

  BoneTransform child_pose;
  child_pose.translation = Vec3(0.0f, 1.5f, 0.0f);
  skeleton.setBonePoseLocal(static_cast<size_t>(child), child_pose);

  skin_data.joint_to_bone = {parent, child};
  skin_data.influences.resize(3);

  skin_data.influences[0].joint_indices = glm::ivec4(0, 0, 0, 0);
  skin_data.influences[0].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
  bind_vertices.push_back({});
  bind_vertices[0].position = Vec3(1.0f, 0.0f, 0.0f);

  skin_data.influences[1].joint_indices = glm::ivec4(1, 0, 0, 0);
  skin_data.influences[1].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
  bind_vertices.push_back({});
  bind_vertices[1].position = Vec3(2.0f, 0.0f, 0.0f);

  skin_data.influences[2].joint_indices = glm::ivec4(0, 1, 0, 0);
  skin_data.influences[2].weights = glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);
  bind_vertices.push_back({});
  bind_vertices[2].position = Vec3(1.5f, 0.0f, 0.0f);
}

void test_cpu_vs_gpu_reference_positions_one_frame() {
  using namespace Blunder;

  Skeleton skeleton;
  MeshSkinData skin_data;
  eastl::vector<MeshVertex> bind_vertices;
  build_two_bone_pose_fixture(skeleton, skin_data, bind_vertices);

  eastl::vector<MeshVertex> cpu_skinned;
  applyCpuSkinning(skeleton, skin_data, bind_vertices, cpu_skinned);

  eastl::vector<Vec3> gpu_reference;
  applyGpuReferenceSkinning(skeleton, skin_data, bind_vertices, gpu_reference);

  expect_true("cpu vs gpu-reference positions (one posed frame)",
              all_positions_near(cpu_skinned, gpu_reference));
}

void test_bone_palette_matches_cpu_joint_matrices() {
  using namespace Blunder;

  Skeleton skeleton;
  MeshSkinData skin_data;
  eastl::vector<MeshVertex> bind_vertices;
  build_two_bone_pose_fixture(skeleton, skin_data, bind_vertices);

  eastl::vector<Mat4> gpu_palette;
  buildGpuBonePalette(skeleton, skin_data, gpu_palette);
  expect_true("palette joint count", gpu_palette.size() == 2);

  for (size_t joint_index = 0; joint_index < gpu_palette.size(); ++joint_index) {
    const int bone_index = skin_data.joint_to_bone[joint_index];
    const Mat4 expected =
        skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(bone_index)) *
        skeleton.getBoneInverseBind(static_cast<size_t>(bone_index));
    bool near = true;
    const float* pa = &gpu_palette[joint_index][0][0];
    const float* pb = &expected[0][0];
    for (int i = 0; i < 16; ++i) {
      if (std::fabs(pa[i] - pb[i]) > k_pose_parity_abs_eps) {
        near = false;
        break;
      }
    }
    expect_true("gpu palette joint matches pose*inverse_bind", near);
  }
}

}  // namespace

int main() {
  test_bone_palette_matches_cpu_joint_matrices();
  test_cpu_vs_gpu_reference_positions_one_frame();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
