#include <cstdio>
#include <cmath>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/render/overlay/navigate_gizmo_layout.h"
#include "runtime/function/render/overlay/navigate_gizmo_shared.h"
#include "runtime/function/render/overlay/navigate_gizmo_style.h"

namespace {

int g_failures = 0;

void expect_near(const char* label, const float actual, const float expected) {
  if (std::fabs(actual - expected) > 1e-3f) {
    std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, actual, expected);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_near("blender default navigate diameter 80",
              navigateGizmoDiameterPx(1.0f), 80.0f);
  const NavigateGizmoLayout layout = computeNavigateGizmoLayout(1920, 1080);
  expect_near("layout size matches diameter", layout.gizmo_size, 80.0f);
  expect_near("positive ball radius blender handle size",
              NavigateGizmoMetrics::kBallRadius / NavigateGizmoMetrics::kBgRadius,
              0.20f);
  expect_near("negative balls match positive size",
              NavigateGizmoMetrics::kNegBallRadius,
              NavigateGizmoMetrics::kBallRadius);
  expect_near("blender arm ends at 0.80",
              NavigateGizmoMetrics::kArmLength, 0.80f);
  expect_near("neg ball border matches plane SDF stroke px",
              NavigateGizmoMetrics::kNegBallBorderWidthPx, 0.3f);

  {
    const glm::vec4 view_bg{0.12f, 0.12f, 0.14f, 1.0f};
    const glm::vec3 axis_rgb = kAxisColorPositiveX;
    const glm::vec4 front = navigateAxisFadingColor(view_bg, axis_rgb, 1.0f);
    expect_near("front axis full red at depth 1", front.r, axis_rgb.x);
    const glm::vec4 back = navigateAxisFadingColor(view_bg, axis_rgb, -1.0f);
    if (back.r >= front.r) {
      std::fprintf(stderr, "FAIL back axis dimmer than front\n");
      ++g_failures;
    }
    const glm::vec4 neg = navigateAxisNegativeColor(axis_rgb);
    expect_near("neg ball opaque muted fill", neg.a, 1.0f);
    const float gray = NavigateGizmoMetrics::kNegBallFillGray;
    const float t = NavigateGizmoMetrics::kNegBallFillMix;
    expect_near("neg fill gray+axis mix r", neg.r, gray * (1.0f - t) + axis_rgb.x * t);
    expect_near("neg fill gray+axis mix g", neg.g, gray * (1.0f - t) + axis_rgb.y * t);
    expect_near("neg fill gray+axis mix b", neg.b, gray * (1.0f - t) + axis_rgb.z * t);
    const glm::vec4 neg_again = navigateAxisNegativeColor(axis_rgb);
    expect_near("neg fill ignores view bg", neg_again.r, neg.r);
  }
  {
    const glm::vec4 backdrop = navigateGizmoHighlightBackdropColor();
    expect_near("hover backdrop gray 0.5", backdrop.r, 0.5f);
    expect_near("hover backdrop alpha 0.5", backdrop.a, 0.5f);
  }

  if (g_failures == 0) {
    std::printf("navigate_gizmo_style_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "navigate_gizmo_style_test: %d failure(s)\n", g_failures);
  return 1;
}
