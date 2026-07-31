#include "runtime/function/render/mesh_preview/mesh_preview_orbit_camera.h"

#include <cstdio>
#include <cmath>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_false(const char* label, bool ok) {
  expect_true(label, !ok);
}

void expect_near(const char* label, float actual, float expected,
                 float epsilon = 1e-3f) {
  if (std::fabs(actual - expected) > epsilon) {
    std::fprintf(stderr, "FAIL %s (got %f want %f)\n", label, actual, expected);
    ++g_failures;
  }
}

Blunder::MeshPreviewCameraFrame makeTestFrame() {
  Blunder::MeshPreviewCameraFrame frame{};
  frame.target = glm::vec3(0.0f, 0.0f, 0.0f);
  frame.eye = glm::vec3(3.0f, 2.0f, 1.5f);
  frame.up = glm::vec3(0.0f, 0.0f, 1.0f);
  frame.vertical_fov_rad = glm::radians(40.0f);
  frame.ok = true;
  return frame;
}

}  // namespace

int main() {
  using namespace Blunder;

  MeshPreviewOrbitCamera camera;
  const MeshPreviewCameraFrame default_frame = makeTestFrame();
  camera.setDefaultFrame(default_frame);

  const MeshPreviewCameraFrame initial = camera.currentFrame();
  expect_true("default frame ok", initial.ok);
  expect_near("default eye x", initial.eye.x, default_frame.eye.x);
  expect_near("default eye y", initial.eye.y, default_frame.eye.y);
  expect_near("default eye z", initial.eye.z, default_frame.eye.z);
  expect_false("no user orbit initially", camera.hasUserOrbit());

  camera.orbit(40.0f, 0.0f);
  const MeshPreviewCameraFrame orbited = camera.currentFrame();
  expect_true("orbit marks user state", camera.hasUserOrbit());
  expect_true("orbit changes eye", glm::length(orbited.eye - default_frame.eye) > 0.01f);
  expect_near("target unchanged after orbit", orbited.target.x, default_frame.target.x);

  camera.reset();
  const MeshPreviewCameraFrame reset_frame = camera.currentFrame();
  expect_false("reset clears user orbit", camera.hasUserOrbit());
  expect_near("reset restores eye x", reset_frame.eye.x, default_frame.eye.x);
  expect_near("reset restores eye y", reset_frame.eye.y, default_frame.eye.y);
  expect_near("reset restores eye z", reset_frame.eye.z, default_frame.eye.z);

  camera.orbit(10.0f, 10.0f);
  expect_true("orbit state ephemeral in memory", camera.hasUserOrbit());
  const MeshPreviewOrbitState orbit_state = camera.orbitState();
  expect_true("orbit yaw offset non-zero", std::fabs(orbit_state.yaw_offset_rad) > 1e-6f);
  MeshPreviewOrbitCamera restarted;
  restarted.setDefaultFrame(default_frame);
  expect_false("new camera session has no persisted orbit", restarted.hasUserOrbit());
  camera.clear();
  expect_false("clear drops orbit state", camera.hasUserOrbit());
  expect_false("clear invalidates frame", camera.currentFrame().ok);

  camera.setDefaultFrame(default_frame);
  const float before_zoom =
      glm::length(camera.currentFrame().eye - camera.currentFrame().target);
  camera.zoom(1.0f);
  const float after_zoom =
      glm::length(camera.currentFrame().eye - camera.currentFrame().target);
  expect_true("zoom changes distance", after_zoom < before_zoom);
  camera.reset();
  expect_near("zoom reset distance",
              glm::length(camera.currentFrame().eye - camera.currentFrame().target),
              before_zoom);

  if (g_failures != 0) {
    std::fprintf(stderr, "mesh_preview_orbit_camera_test: %d failure(s)\n",
                 g_failures);
    return 1;
  }

  std::fprintf(stdout, "mesh_preview_orbit_camera_test: all passed\n");
  std::fflush(stdout);
  return 0;
}
