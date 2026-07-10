#include <cstdio>

#include "runtime/function/render/pick/pick_instance_buffer.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/mesh_asset.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool condition) {
  if (!condition) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_eq_u32(const char* label, uint32_t actual, uint32_t expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s: expected %u got %u\n", label, expected, actual);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  SceneInstance scene;
  const EntityId root = scene.createEntity("Box", Vec3{}, Quat{}, Vec3{});

  MeshVertex v0{};
  MeshVertex v1{};
  v0.position = glm::vec3(0.0f, 0.0f, 0.0f);
  v1.position = glm::vec3(1.0f, 1.0f, 1.0f);

  MeshRendererComponent renderer{};
  renderer.mesh = eastl::make_shared<MeshAsset>(
      Asset::Meta{}, eastl::vector<MeshVertex>{v0, v1}, eastl::vector<uint32_t>{});
  scene.setMeshRenderer(root, eastl::move(renderer));

  PickInstanceBuffer buffer;
  buffer.rebuild(scene, nullptr);

  expect_eq_u32("gpu instance count", static_cast<uint32_t>(buffer.gpuInstances().size()),
                1u);
  expect_true("no gpu draws without RenderSystem",
              buffer.pickDraws().empty());

  const PickInstanceGpu& row = buffer.gpuInstances().front();
  expect_eq_u32("entity_id", row.entity_id, static_cast<uint32_t>(root));
  expect_eq_u32("parent_id", row.parent_id, static_cast<uint32_t>(k_invalid_entity_id));
  expect_true("pickable flag", (row.flags & k_pick_instance_pickable) != 0u);
  expect_true("aabb min x", row.aabb_min[0] >= 0.0f && row.aabb_min[0] <= 0.01f);
  expect_true("aabb max x", row.aabb_max[0] >= 0.99f && row.aabb_max[0] <= 1.01f);

  if (g_failures == 0) {
    std::printf("pick_instance_buffer_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "pick_instance_buffer_test: %d failure(s)\n", g_failures);
  return 1;
}
