#include <cstdio>
#include <cmath>

#include "runtime/function/render/overlay/outline_aa.h"

int main() {
  using namespace Blunder;
  int failures = 0;
  auto expect_near = [&](const char* label, const float a, const float b) {
    if (std::fabs(a - b) > 1e-4f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };

  expect_near("interior coverage 0", outlineEdgeCoverage(/*neighbor_edge_count=*/0), 0.0f);
  expect_near("full silhouette coverage 1", outlineEdgeCoverage(8), 1.0f);

  const float partial = outlineEdgeCoverage(2);
  if (partial <= 0.0f || partial >= 1.0f) {
    std::fprintf(stderr, "FAIL partial edge in (0,1)\n");
    ++failures;
  }

  expect_near("smooth min", kOutlineEdgeSmoothMin, 0.25f);
  expect_near("smooth max", kOutlineEdgeSmoothMax, 2.5f);

  const float diag = outlineEdgeCoverage(3);
  if (diag < 0.25f) {
    std::fprintf(stderr, "FAIL diagonal edge too faint (got %f)\n", diag);
    ++failures;
  }

  const float two = outlineEdgeCoverage(2);
  if (two <= 0.0f || two >= 1.0f) {
    std::fprintf(stderr, "FAIL two-neighbor shallow diagonal partial (got %f)\n", two);
    ++failures;
  }

  if (failures == 0) {
    std::printf("outline_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
