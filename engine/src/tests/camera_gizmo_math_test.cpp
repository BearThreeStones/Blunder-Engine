#include "runtime/function/render/overlay/camera_gizmo_math.h"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void expect_near(const char* label, float actual, float expected, float eps = 1e-4f) {
  if (std::abs(actual - expected) > eps) {
    std::fprintf(stderr, "FAIL %s: got %f expected %f\n", label, actual, expected);
    ++g_failures;
  }
}

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_near("clamp low", clampCameraFovDegrees(0.5f), kMinCameraFovDegrees);
  expect_near("clamp high", clampCameraFovDegrees(200.0f), kMaxCameraFovDegrees);

  const float distance = 1.0f;
  const float fov = 60.0f;
  const float half_h = halfHeightFromVerticalFovDegrees(fov, distance);
  const float round_trip = verticalFovDegreesFromHalfHeight(half_h, distance);
  expect_near("fov round trip", round_trip, fov, 1e-3f);

  float near_clip = 0.1f;
  float far_clip = 100.0f;
  setCameraNearClip(near_clip, far_clip, 0.5f);
  expect_near("near set", near_clip, 0.5f);
  expect_true("far preserved", far_clip >= near_clip + kCameraClipEpsilon);

  near_clip = 0.1f;
  far_clip = 100.0f;
  setCameraFarClip(near_clip, far_clip, 2.0f);
  expect_near("far set", far_clip, 2.0f);
  expect_true("near preserved", near_clip == 0.1f);

  near_clip = 5.0f;
  far_clip = 5.0f;
  setCameraNearClip(near_clip, far_clip, 8.0f);
  expect_true("far pushed by near", far_clip >= near_clip + kCameraClipEpsilon);

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("camera_gizmo_math_test: all passed\n");
  return 0;
}
