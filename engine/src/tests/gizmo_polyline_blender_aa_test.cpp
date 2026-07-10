#include <cstdio>
#include <cmath>

#include "runtime/function/render/gizmo/gizmo_polyline_aa.h"

int main() {
  using namespace Blunder;
  int failures = 0;
  auto expect_near = [&](const char* label, const float a, const float b) {
    if (std::fabs(a - b) > 1e-3f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };

  expect_near("blender center", polylineStrokeAlphaBlender(0.0f, 2.0f, true), 1.0f);

  const float half_w = (2.0f + kPolylineSmoothWidth) * 0.5f;
  expect_near("blender edge", polylineStrokeAlphaBlender(half_w, 2.0f, true), 0.0f);

  const float mid = polylineStrokeAlphaBlender(half_w * 0.8f, 2.0f, true);
  if (mid <= 0.0f || mid >= 1.0f) {
    std::fprintf(stderr, "FAIL blender mid partial (got %f)\n", mid);
    ++failures;
  }

  if (failures == 0) {
    std::printf("gizmo_polyline_blender_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
