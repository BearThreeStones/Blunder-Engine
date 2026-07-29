#include "runtime/core/object/skeleton.h"
#include "runtime/function/scene/cpu_skinning.h"
#include "runtime/function/scene/gpu_skinning.h"
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

bool mat4_near(const Blunder::Mat4& a, const Blunder::Mat4& b, float eps = 1e-4f) {
  const float* pa = &a[0][0];
  const float* pb = &b[0][0];
  for (int i = 0; i < 16; ++i) {
    if (std::fabs(pa[i] - pb[i]) > eps) {
      return false;
    }
  }
  return true;
}

void gpu_palette_matches_cpu_joint_matrix() {
  using namespace Blunder;

  Skeleton skeleton;
  const int bone = skeleton.addBone("root", -1);
  skeleton.setBoneInverseBind(static_cast<size_t>(bone), Mat4(1.0f));
  BoneTransform pose;
  pose.translation = Vec3(0.0f, 3.0f, 0.0f);
  skeleton.setBonePoseLocal(static_cast<size_t>(bone), pose);

  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0};
  skin_data.influences.push_back({});
  skin_data.influences[0].joint_indices = glm::ivec4(0, 0, 0, 0);
  skin_data.influences[0].weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

  eastl::vector<Mat4> gpu_palette;
  buildGpuBonePalette(skeleton, skin_data, gpu_palette);
  expect_true("gpu palette has one joint", gpu_palette.size() == 1);

  Mat4 expected =
      skeleton.getBoneGlobalPoseMatrix(0) * skeleton.getBoneInverseBind(0);
  expect_true("gpu palette matches pose*inverse_bind",
              mat4_near(gpu_palette[0], expected));
}

void cooked_final_flag_routes_gpu_path() {
  using namespace Blunder;

  Asset::Meta meta;
  meta.virtual_path = "assets/Meshes/test.mesh.yaml";
  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0};
  skin_data.influences.push_back({});

  MeshAsset cooked(meta, {}, {}, {}, nullptr, skin_data, true);
  expect_true("cooked final skin enables gpu path",
              shouldUseGpuSkinning(cooked));

  MeshAsset intermediate(meta, {}, {}, {}, nullptr, skin_data, false);
  expect_true("intermediate skin stays on cpu path",
              !shouldUseGpuSkinning(intermediate));
}

void pack_skinned_vertices_preserves_influences() {
  using namespace Blunder;

  Asset::Meta meta;
  eastl::vector<MeshVertex> vertices(1);
  vertices[0].position = Vec3(1.0f, 2.0f, 3.0f);
  MeshSkinData skin_data;
  skin_data.joint_to_bone = {0, 1};
  skin_data.influences.push_back({});
  skin_data.influences[0].joint_indices = glm::ivec4(0, 1, 0, 0);
  skin_data.influences[0].weights = glm::vec4(0.6f, 0.4f, 0.0f, 0.0f);

  MeshAsset mesh(meta, vertices, {}, {}, nullptr, skin_data, true);
  eastl::vector<SkinnedMeshVertex> packed;
  packSkinnedMeshVertices(mesh, packed);

  expect_true("packed vertex count", packed.size() == 1);
  expect_true("packed position preserved",
              packed[0].position.x == 1.0f && packed[0].position.y == 2.0f);
  expect_true("packed joint indices",
              packed[0].joint_indices.x == 0 && packed[0].joint_indices.y == 1);
  expect_true("packed weights", packed[0].weights.x == 0.6f);
}

}  // namespace

int main() {
  gpu_palette_matches_cpu_joint_matrix();
  cooked_final_flag_routes_gpu_path();
  pack_skinned_vertices_preserves_influences();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  return 0;
}
