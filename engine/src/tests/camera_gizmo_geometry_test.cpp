#include "runtime/function/render/overlay/camera_gizmo_geometry.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr float kEps = 1e-5f;

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

void expect_vec3_near(const Blunder::Vec3& actual, const Blunder::Vec3& expected,
                      const char* label) {
  if (glm::length(actual - expected) > kEps) {
    std::fprintf(stderr, "FAIL %s: expected (%.6f, %.6f, %.6f) got (%.6f, %.6f, %.6f)\n",
                 label, expected.x, expected.y, expected.z, actual.x, actual.y, actual.z);
    ++g_failures;
  }
}

float frame_width(const Blunder::CameraGizmoFrame& frame) {
  return frame.corners[1].x - frame.corners[0].x;
}

float frame_height(const Blunder::CameraGizmoFrame& frame) {
  return frame.corners[0].y - frame.corners[3].y;
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_near(kCameraGizmoDisplayDistance, 1.0f, "default display distance constant");

  const float display_distance = 1.0f;
  const float vertical_fov = glm::radians(60.0f);
  const float aspect = 16.0f / 9.0f;

  const CameraGizmoFrame frame =
      buildCameraGizmoFrameLocal(vertical_fov, aspect, display_distance);

  expect_vec3_near(frame.origin, Vec3(0.0f), "origin at local zero");

  const float half_h = display_distance * std::tan(vertical_fov * 0.5f);
  const float half_w = half_h * aspect;
  const float z = -display_distance;

  expect_vec3_near(frame.corners[0], Vec3(-half_w, half_h, z), "top-left corner");
  expect_vec3_near(frame.corners[1], Vec3(half_w, half_h, z), "top-right corner");
  expect_vec3_near(frame.corners[2], Vec3(half_w, -half_h, z), "bottom-right corner");
  expect_vec3_near(frame.corners[3], Vec3(-half_w, -half_h, z), "bottom-left corner");

  expect_true("16:9 frame wider than tall", frame_width(frame) > frame_height(frame));

  const CameraGizmoFrame narrow_fov =
      buildCameraGizmoFrameLocal(glm::radians(30.0f), aspect, display_distance);
  const CameraGizmoFrame wide_fov =
      buildCameraGizmoFrameLocal(glm::radians(90.0f), aspect, display_distance);
  expect_true("larger FOV yields wider frame",
              frame_width(wide_fov) > frame_width(narrow_fov));
  expect_true("larger FOV yields taller frame",
              frame_height(wide_fov) > frame_height(narrow_fov));

  expect_true("up triangle tip above top edge", frame.up_triangle[1].y > half_h);
  expect_vec3_near(frame.up_triangle[0], frame.corners[0], "up triangle left base at TL");
  expect_vec3_near(frame.up_triangle[2], frame.corners[1], "up triangle right base at TR");

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("camera_gizmo_geometry_test: all passed\n");
  return 0;
}
