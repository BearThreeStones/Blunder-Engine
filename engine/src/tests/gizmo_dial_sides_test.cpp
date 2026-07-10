#include <cstdio>

#include <cmath>

#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/render/gizmo/transform_gizmo_types.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {

int g_failures = 0;

void expect_eq(const uint32_t actual, const uint32_t expected, const char* label) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s: expected %u got %u\n", label, expected, actual);
    ++g_failures;
  }
}

void expect_ge(const uint32_t actual, const uint32_t min_value, const char* label) {
  if (actual < min_value) {
    std::fprintf(stderr, "FAIL %s: expected >= %u got %u\n", label, min_value, actual);
    ++g_failures;
  }
}

void expect_near(const float actual, const float expected, const float epsilon,
                 const char* label) {
  if (std::fabs(actual - expected) > epsilon) {
    std::fprintf(stderr, "FAIL %s: expected ~%.2f got %.2f\n", label, expected, actual);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using M = Blunder::TransformGizmoMetrics;

  expect_eq(Blunder::computeDialSides(0.0f), M::k_dial_sides_min, "tiny radius");
  expect_eq(Blunder::computeDialSides(10.0f), M::k_dial_sides_min, "small radius");

  const uint32_t medium = Blunder::computeDialSides(80.0f);
  expect_ge(medium, 200u, "medium radius");

  const uint32_t large = Blunder::computeDialSides(400.0f);
  expect_eq(large, M::k_dial_sides_max, "clamped at max");

  const glm::mat4 view(1.0f);
  const glm::mat4 proj = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, -1.0f, 1.0f);
  glm::mat4 gizmo_world(1.0f);
  gizmo_world[0][0] = 50.0f;
  gizmo_world[1][1] = 50.0f;
  expect_near(Blunder::computeRingScreenRadiusPx(view, proj, gizmo_world, 100.0f, 100.0f),
              50.0f, 1.0f, "projected screen radius");

  if (g_failures == 0) {
    std::printf("gizmo_dial_sides_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "gizmo_dial_sides_test: %d failure(s)\n", g_failures);
  return 1;
}
