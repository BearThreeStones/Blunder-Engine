#include <cstdio>
#include <cmath>

#include "runtime/function/render/gizmo/gizmo_sdf_aa.h"

int main() {
  using namespace Blunder;
  int failures = 0;
  auto expect_near = [&](const char* label, const float a, const float b) {
    if (std::fabs(a - b) > 1e-3f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };

  expect_near("disc interior", discFillAlpha(-2.0f, 1.25f), 1.0f);
  expect_near("disc exterior", discFillAlpha(2.0f, 1.25f), 0.0f);

  expect_near("segment on line", sdSegment2d(0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 10.0f), 0.0f);
  expect_near("segment perp", sdSegment2d(3.0f, 5.0f, 0.0f, 0.0f, 0.0f, 10.0f), 3.0f);

  const float on_stroke =
      segmentLineAlpha(0.0f, 5.0f, 0.0f, 0.0f, 0.0f, 10.0f, 2.0f, 1.0f);
  expect_near("segment stroke center", on_stroke, 1.0f);

  const float far_stroke =
      segmentLineAlpha(10.0f, 5.0f, 0.0f, 0.0f, 0.0f, 10.0f, 2.0f, 1.0f);
  expect_near("segment stroke far", far_stroke, 0.0f);

  const float box_center = sdBox2d(0.0f, 0.0f, 0.1f, 0.1f);
  if (box_center >= 0.0f) {
    std::fprintf(stderr, "FAIL box center inside (got %f want < 0)\n", box_center);
    ++failures;
  }

  expect_near("box outside corner", sdBox2d(0.2f, 0.2f, 0.1f, 0.1f), 0.141421f);

  const float on_rect_edge =
      rectRingAlpha(0.1f, 0.0f, 0.1f, 0.1f, 2.0f, 1.0f);
  expect_near("rect border on edge", on_rect_edge, 1.0f);

  const float far_rect_border =
      rectRingAlpha(5.0f, 5.0f, 0.1f, 0.1f, 2.0f, 1.0f);
  expect_near("rect border far", far_rect_border, 0.0f);

  const float fill_center = rectFillAlpha(0.0f, 0.0f, 0.1f, 0.1f, 1.0f);
  expect_near("rect fill interior", fill_center, 0.85f);

  const float fill_outside = rectFillAlpha(5.0f, 5.0f, 0.1f, 0.1f, 1.0f);
  expect_near("rect fill exterior", fill_outside, 0.0f);

  if (failures == 0) {
    std::printf("gizmo_sdf_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
