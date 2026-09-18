#include "runtime/function/scene/gltf_collision_extras.h"

#include <cstdio>
#include <cstring>

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

  {
    const char* json =
        "{\"collision_info\":{\"triangles\":["
        "{\"v0\":[-10,-10,0],\"v1\":[10,-10,0],\"v2\":[0,10,0]}"
        "]}}";
    ColliderComponent collider{};
    expect_true("present extras attach",
                parseCollisionExtrasJson(json, std::strlen(json), collider));
    expect_true("static trimesh",
                collider.shape == ColliderShapeKind::TriangleMesh &&
                    collider.body_kind == ColliderBodyKind::Static);
    expect_true("one triangle", collider.triangles.size() == 1);
    expect_true("v0 x", collider.triangles[0].v0.x == -10.0f);
  }

  {
    const char* json = "{\"collision_info\":{}}";
    ColliderComponent collider{};
    expect_true("empty extras skip",
                !parseCollisionExtrasJson(json, std::strlen(json), collider));
  }

  {
    const char* json = "{\"name\":\"Mesh\"}";
    ColliderComponent collider{};
    expect_true("missing extras skip",
                !parseCollisionExtrasJson(json, std::strlen(json), collider));
  }

  {
    ColliderComponent collider{};
    expect_true("null skip", !parseCollisionExtrasJson(nullptr, 0, collider));
    expect_true("zero length skip", !parseCollisionExtrasJson("", 0, collider));
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d gltf_collision_extras_test failure(s)\n", g_failures);
    return 1;
  }
  std::printf("gltf_collision_extras_test: all passed\n");
  return 0;
}
