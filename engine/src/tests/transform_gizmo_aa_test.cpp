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

  expect_near("stroke center full alpha", polylineStrokeAlpha(0.0f, 0.1f), 1.0f);
  expect_near("stroke edge zero alpha", polylineStrokeAlpha(1.0f, 0.1f), 0.0f);

  const float mid = polylineStrokeAlpha(0.92f, 0.1f);
  if (mid <= 0.0f || mid >= 1.0f) {
    std::fprintf(stderr, "FAIL mid stroke partial alpha (got %f)\n", mid);
    ++failures;
  }

  if (failures == 0) {
    std::printf("transform_gizmo_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
