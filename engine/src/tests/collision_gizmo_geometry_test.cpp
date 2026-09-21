#include "runtime/function/render/overlay/collision_gizmo_geometry.h"

#include <cstdio>

namespace {
int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}
}  // namespace

int main() {
  using namespace Blunder;

  ColliderComponent box{};
  box.shape = ColliderShapeKind::Box;
  box.box_half_extents = Vec3(1.0f, 2.0f, 3.0f);

  int box_edges = 0;
  forEachColliderWireSegment(box, [&](const Vec3&, const Vec3&) { ++box_edges; });
  expect_true("box emits 12 edges", box_edges == 12);

  ColliderComponent mesh{};
  mesh.shape = ColliderShapeKind::TriangleMesh;
  ColliderTriangle tri{};
  tri.v0 = Vec3(-1, -1, 0);
  tri.v1 = Vec3(1, -1, 0);
  tri.v2 = Vec3(0, 1, 0);
  mesh.triangles.push_back(tri);
  int tri_edges = 0;
  forEachColliderWireSegment(mesh, [&](const Vec3&, const Vec3&) { ++tri_edges; });
  expect_true("trimesh emits triangle edges", tri_edges == 3);

  CharacterControllerComponent cct{};
  int capsule_edges = 0;
  forEachCharacterControllerWireSegment(
      cct, [&](const Vec3&, const Vec3&) { ++capsule_edges; });
  expect_true("cct capsule emits segments", capsule_edges > 0);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("collision_gizmo_geometry_test: all passed\n");
  return 0;
}
