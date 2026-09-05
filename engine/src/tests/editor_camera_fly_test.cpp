#include "runtime/function/render/editor_camera.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  EditorCamera camera(nullptr);
  expect_true("no button is not viewport interacting",
              !camera.isViewportInteracting());

  const Vec3 before = camera.getFocalPoint();
  camera.onUpdate(0.25f);
  expect_true("hover without RMB/MMB does not fly",
              before == camera.getFocalPoint());

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::fprintf(stderr, "editor_camera_fly_test: all passed\n");
  return 0;
}
