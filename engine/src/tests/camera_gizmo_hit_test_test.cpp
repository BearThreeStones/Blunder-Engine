#include "runtime/function/render/overlay/camera_gizmo_hit_test.h"

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

void expect_near(float actual, float expected, const char* label) {
  if (std::fabs(actual - expected) > kEps) {
    std::fprintf(stderr, "FAIL %s: expected %.6f got %.6f\n", label, expected, actual);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_near(distanceToSegment2D(glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 0.0f),
                                  glm::vec2(2.0f, 2.0f)),
              0.0f, "point on segment");
  expect_near(distanceToSegment2D(glm::vec2(2.0f, 0.0f), glm::vec2(0.0f, 0.0f),
                                  glm::vec2(2.0f, 2.0f)),
              std::sqrt(2.0f), "point off segment");
  expect_near(distanceToSegment2D(glm::vec2(3.0f, 4.0f), glm::vec2(1.0f, 1.0f),
                                  glm::vec2(1.0f, 1.0f)),
              std::sqrt(13.0f), "degenerate segment");

  const float vp_w = 800.0f;
  const float vp_h = 600.0f;
  const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
  const glm::mat4 proj =
      glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, 0.1f, 100.0f);

  const std::optional<glm::vec2> center =
      projectWorldToViewportLocal(glm::vec3(0.0f), view, proj, vp_w, vp_h);
  expect_true("world origin projects", center.has_value());
  if (center.has_value()) {
    expect_near(center->x, vp_w * 0.5f, "projected x center");
    expect_near(center->y, vp_h * 0.5f, "projected y center");
  }

  const CameraGizmoFrame frame =
      buildCameraGizmoFrameLocal(glm::radians(60.0f), vp_w / vp_h, 1.0f);
  const glm::mat4 world{1.0f};
  const glm::vec2 miss_pointer(12.0f, 12.0f);
  const std::optional<float> miss = hitTestCameraGizmoFrameViewportLocal(
      miss_pointer, frame, world, view, proj, vp_w, vp_h, 2.0f);
  expect_true("corner misses frame at 2px threshold", !miss.has_value());

  const glm::vec2 hit_pointer(vp_w * 0.5f, vp_h * 0.5f);
  const std::optional<float> hit = hitTestCameraGizmoFrameViewportLocal(
      hit_pointer, frame, world, view, proj, vp_w, vp_h, 200.0f);
  expect_true("center hits frame at wide threshold", hit.has_value());

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("camera_gizmo_hit_test_test: all passed\n");
  return 0;
}
