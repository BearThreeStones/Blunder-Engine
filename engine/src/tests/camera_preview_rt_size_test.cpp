#include "runtime/function/render/overlay/camera_preview_rt_size.h"

#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

void expect_eq_u32(const char* label, uint32_t actual, uint32_t expected) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL %s (got %u want %u)\n", label, actual, expected);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  expect_eq_u32("kCameraPreviewMaxLongEdgePx", kCameraPreviewMaxLongEdgePx, 480u);

  {
    const CameraPreviewRtSize result = computeCameraPreviewRtSize(320.0f, 180.0f);
    expect_true("320x180 -> ok", result.ok);
    expect_eq_u32("320x180 width", result.width, 320u);
    expect_eq_u32("320x180 height", result.height, 180u);
  }

  {
    const CameraPreviewRtSize result = computeCameraPreviewRtSize(1920.0f, 1080.0f);
    expect_true("1920x1080 -> ok", result.ok);
    expect_eq_u32("1920x1080 long edge clamped width", result.width, 480u);
    expect_eq_u32("1920x1080 aspect preserved height", result.height, 270u);
    expect_true("1920x1080 long edge <= max",
                result.width <= kCameraPreviewMaxLongEdgePx &&
                    result.height <= kCameraPreviewMaxLongEdgePx);
    expect_true("1920x1080 max dimension is long edge",
                result.width == kCameraPreviewMaxLongEdgePx);
  }

  {
    const CameraPreviewRtSize result = computeCameraPreviewRtSize(0.0f, 0.0f);
    expect_true("0x0 -> !ok", !result.ok);
    expect_eq_u32("0x0 width", result.width, 0u);
    expect_eq_u32("0x0 height", result.height, 0u);
  }

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("camera_preview_rt_size_test: all passed\n");
  return 0;
}
