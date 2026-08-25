#include "runtime/function/render/overlay/light_gizmo_hit_test.h"
#include "runtime/function/render/overlay/overlay_gizmo_pick.h"

#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr float kEps = 1e-4f;

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

  const OverlayGizmoPickHit camera_hit{EntityId{1}, -5.0f};
  const OverlayGizmoPickHit light_hit{EntityId{2}, -2.0f};
  const std::optional<OverlayGizmoPickHit> closer = pickCloserOverlayGizmoHit(
      camera_hit, light_hit);
  expect_true("closer light wins vs camera",
              closer.has_value() && closer->entity_id == EntityId{2});

  const OverlayGizmoPickHit near_camera{EntityId{3}, -1.0f};
  const std::optional<OverlayGizmoPickHit> camera_wins = pickCloserOverlayGizmoHit(
      near_camera, light_hit);
  expect_true("closer camera wins vs light",
              camera_wins.has_value() && camera_wins->entity_id == EntityId{3});

  expect_true("only light hit",
              pickCloserOverlayGizmoHit(std::nullopt, light_hit).has_value() &&
                  pickCloserOverlayGizmoHit(std::nullopt, light_hit)->entity_id ==
                      EntityId{2});

  const float vp_w = 800.0f;
  const float vp_h = 600.0f;
  const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::mat4 proj =
      glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, 0.1f, 100.0f);
  const glm::mat4 world{1.0f};
  const glm::vec2 center(vp_w * 0.5f, vp_h * 0.5f);

  LightGizmoShape directional{};
  directional.kind = LightGizmoKind::directional;
  const std::optional<float> dir_hit = hitTestLightGizmoViewportLocal(
      center, directional, world, view, proj, vp_w, vp_h, 200.0f);
  expect_true("directional origin hit", dir_hit.has_value());

  LightGizmoShape point{};
  point.kind = LightGizmoKind::point;
  point.range = 1.0f;
  const glm::vec2 miss_pointer(12.0f, 12.0f);
  const std::optional<float> point_miss = hitTestLightGizmoViewportLocal(
      miss_pointer, point, world, view, proj, vp_w, vp_h, 2.0f);
  expect_true("point miss far from sphere", !point_miss.has_value());

  LightGizmoShape area{};
  area.kind = LightGizmoKind::area;
  area.width = 2.0f;
  area.height = 2.0f;
  const std::optional<float> area_hit = hitTestLightGizmoViewportLocal(
      center, area, world, view, proj, vp_w, vp_h, 40.0f);
  expect_true("area rect hit near origin", area_hit.has_value());

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("light_gizmo_hit_test_test: all passed\n");
  return 0;
}
