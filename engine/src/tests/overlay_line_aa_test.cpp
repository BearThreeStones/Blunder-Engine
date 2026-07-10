#include <cstdio>
#include <cmath>

#include <glm/vec2.hpp>

#include "runtime/function/render/overlay/overlay_line_aa.h"

int main() {
  using namespace Blunder;
  int failures = 0;
  auto expect_near = [&](const char* label, const float a, const float b) {
    if (std::fabs(a - b) > 1e-3f) {
      std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, a, b);
      ++failures;
    }
  };

  const LineData center = decodeLineData(glm::vec2(0.0f, 0.5f));
  expect_near("center dist", center.dist, 0.0f);
  expect_near("center dist_raw", center.dist_raw, 0.5f);

  const float cov = lineCoverage(0.0f, 0.0f, true);
  if (cov < 0.97f) {
    std::fprintf(stderr, "FAIL center coverage high (got %f)\n", cov);
    ++failures;
  }

  const float edge = lineCoverage(1.5f, 0.0f, true);
  if (edge > 0.01f) {
    std::fprintf(stderr, "FAIL far edge coverage zero (got %f)\n", edge);
    ++failures;
  }

  {
    const LineData horizontal = decodeLineData(glm::vec2(1.0f, 0.5f));
    const float nd = neighborLineDist(horizontal, glm::ivec2(1, 0));
    expect_near("neighbor horizontal dist", nd, -1.0f);
  }

  if (failures == 0) {
    std::printf("overlay_line_aa_test: all passed\n");
    return 0;
  }
  return 1;
}
