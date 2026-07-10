#include <cstdio>

#include "runtime/function/render/pick/pick_broad_hits.h"
#include "runtime/function/scene/scene_instance.h"

namespace {

int g_failures = 0;

void expect_eq_f(const char* label, float actual, float expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s: expected %f got %f\n", label, expected, actual);
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

  eastl::vector<BroadHit> hits;
  hits.push_back(BroadHit{EntityId{1}, 3.0f});
  hits.push_back(BroadHit{EntityId{2}, 1.0f});
  hits.push_back(BroadHit{EntityId{3}, 2.0f});

  const eastl::vector<BroadHit> sorted = sortBroadHitsByDistance(eastl::move(hits));
  if (sorted.size() != 3u) {
    std::fprintf(stderr, "FAIL sort size\n");
    ++g_failures;
  } else {
    expect_eq_f("sort t0", sorted[0].t, 1.0f);
    expect_eq_f("sort t1", sorted[1].t, 2.0f);
    expect_eq_f("sort t2", sorted[2].t, 3.0f);
  }

  SceneInstance scene;
  const EntityId box_front = scene.createEntity("BoxFront", Vec3{}, Quat{}, Vec3{});
  const EntityId node_a = scene.createEntity("node_a", Vec3{}, Quat{}, Vec3{}, box_front);
  const EntityId leaf_a =
      scene.createEntity("leaf_a", Vec3{}, Quat{}, Vec3{}, node_a);
  const EntityId node_b = scene.createEntity("node_b", Vec3{}, Quat{}, Vec3{}, box_front);
  const EntityId leaf_b =
      scene.createEntity("leaf_b", Vec3{}, Quat{}, Vec3{}, node_b);

  eastl::vector<BroadHit> broad_hits;
  broad_hits.push_back(BroadHit{leaf_a, 1.0f});
  broad_hits.push_back(BroadHit{leaf_b, 2.0f});

  const eastl::vector<EntityId> promoted =
      promoteAndDedupeBroadHits(scene, broad_hits);
  expect_eq_u32("promoted dedupe size", static_cast<uint32_t>(promoted.size()), 1u);
  expect_eq_u32("promoted entity", static_cast<uint32_t>(promoted.front()),
                static_cast<uint32_t>(box_front));

  if (g_failures == 0) {
    std::printf("pick_broad_hits_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "pick_broad_hits_test: %d failure(s)\n", g_failures);
  return 1;
}
