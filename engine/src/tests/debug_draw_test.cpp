#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/render/overlay/debug_draw.h"
#include "runtime/function/render/overlay/debug_draw_geometry.h"
#include "runtime/function/render/overlay/editor_overlay_policy.h"
#include "runtime/function/render/overlay/overlay_state.h"
#include "runtime/function/scene/gltf_unit_scale.h"

#include <cmath>
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

  DebugDraw::reset();
  const Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
  DebugDraw::line(Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), red);
  DebugDraw::wireBox(Vec3(0.0f), Vec3(1.0f), red);
  DebugDraw::wireSphere(Vec3(0.0f), 1.0f, red);
  expect_true("three primitives queued", DebugDraw::commandCount() == 3u);

  OverlayState state{};
  state.camera_position = Vec3(0.0f, -4.0f, 0.0f);
  state.viewport_width = 1920;
  state.viewport_height = 1080;
  state.vertical_fov = 0.785398163f;
  state.is_perspective = true;

  GizmoLineCommandStream stream;
  DebugDraw::expandLines(stream, state);
  expect_true("line + box expand to more than 3 segments", stream.size() > 3u);
  expect_true("box contributes 12 edges among expanded lines",
              stream.size() >= 1u + 12u);
  expect_true("x-ray path is two draws not per-segment",
              debugDrawGpuDrawCalls(stream.size()) == 2u);
  expect_true("empty expand issues no draws", debugDrawGpuDrawCalls(0u) == 0u);

  DebugDraw::endFrame(0.016f);
  expect_true("duration 0 drops after endFrame", DebugDraw::commandCount() == 0u);

  DebugDraw::line(Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), red, 1.0f);
  expect_true("persist command stays this frame", DebugDraw::commandCount() == 1u);
  DebugDraw::endFrame(0.4f);
  expect_true("persist survives partial duration", DebugDraw::commandCount() == 1u);
  DebugDraw::endFrame(0.7f);
  expect_true("persist expires after duration", DebugDraw::commandCount() == 0u);

  const int near_steps = debugDrawCircleSteps(
      2.0f, 1.0f, 1080.0f, 0.785398163f, 10.0f, true);
  const int far_steps = debugDrawCircleSteps(
      2.0f, 80.0f, 1080.0f, 0.785398163f, 10.0f, true);
  expect_true("nearby sphere uses more segments than distant",
              near_steps > far_steps);
  expect_true("steps stay in adaptive range",
              near_steps >= k_debug_draw_min_circle_steps &&
                  far_steps <= k_debug_draw_max_circle_steps);

  expect_true("editor always shows debug draw",
              debugDrawVisible(EngineHostMode::Editor, false));
  expect_true("player defaults off",
              !debugDrawVisible(EngineHostMode::Player, false));
  expect_true("player InGame switch enables draw",
              debugDrawVisible(EngineHostMode::Player, true));

  expect_true("metre world is 1:1", gltfMetreQuantityInWorld(1.0f, false) == 1.0f);
  expect_true("centimetre world scales metres like light range",
              std::fabs(gltfMetreQuantityInWorld(1.0f, true) - 125.0f) < 0.01f);

  DebugDraw::reset();
  expect_true("ingame defaults off after reset", !DebugDraw::inGameEnabled());
  const int abi_rc = blunder_debug_draw_line(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                             1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
  expect_true("c-abi line ok", abi_rc == BLUNDER_ENGINE_OK);
  expect_true("c-abi line enqueues world units when no scene",
              DebugDraw::commandCount() == 1u);

  expect_true("c-abi version 15", BLUNDER_ENGINE_C_ABI_VERSION == 15);

  DebugDraw::reset();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d test(s) failed\n", g_failures);
    return 1;
  }
  std::fprintf(stdout, "debug_draw_test: all passed\n");
  return 0;
}
