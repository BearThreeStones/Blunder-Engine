#include <cstdio>

#include "runtime/function/render/pick/pick_entity_promotion.h"
#include "runtime/function/scene/scene_instance.h"

namespace {

int g_failures = 0;

void expect_eq(const char* label, Blunder::EntityId actual,
               Blunder::EntityId expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s: expected %u got %u\n", label,
                 static_cast<unsigned>(expected),
                 static_cast<unsigned>(actual));
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  SceneInstance scene;

  const EntityId box_front = scene.createEntity("BoxFront", Vec3{}, Quat{}, Vec3{});
  const EntityId node = scene.createEntity("node", Vec3{}, Quat{}, Vec3{}, box_front);
  const EntityId node_prim0 =
      scene.createEntity("node_prim0", Vec3{}, Quat{}, Vec3{}, node);
  expect_eq("gltf leaf promotes to scene root", promotePickEntity(scene, node_prim0),
            box_front);

  const EntityId root_mesh =
      scene.createEntity("RootMesh", Vec3{}, Quat{}, Vec3{});
  expect_eq("root mesh unchanged", promotePickEntity(scene, root_mesh), root_mesh);

  const EntityId parent = scene.createEntity("Parent", Vec3{}, Quat{}, Vec3{});
  const EntityId child =
      scene.createEntity("ChildMesh", Vec3{}, Quat{}, Vec3{}, parent);
  expect_eq("two-level promotes to parent", promotePickEntity(scene, child), parent);

  expect_eq("invalid leaf", promotePickEntity(scene, k_invalid_entity_id),
            k_invalid_entity_id);

  const EntityId scene_root = scene.createEntity("SceneRoot", Vec3{}, Quat{}, Vec3{});
  const EntityId group =
      scene.createEntity("Group", Vec3{}, Quat{}, Vec3{}, scene_root);
  const EntityId deep_leaf =
      scene.createEntity("DeepLeaf", Vec3{}, Quat{}, Vec3{}, group);
  expect_eq("three-level promotes to scene root child",
            promotePickEntity(scene, deep_leaf), scene_root);

  eastl::vector<EntityId> leaves;
  leaves.push_back(node_prim0);
  leaves.push_back(child);
  const eastl::vector<EntityId> deduped = dedupePromotedPickHits(scene, leaves);
  if (deduped.size() != 2u) {
    std::fprintf(stderr, "FAIL dedupe size: expected 2 got %zu\n", deduped.size());
    ++g_failures;
  }

  if (g_failures == 0) {
    std::printf("pick_entity_promotion_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "pick_entity_promotion_test: %d failure(s)\n", g_failures);
  return 1;
}
