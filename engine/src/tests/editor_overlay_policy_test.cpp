#include "runtime/function/global/engine_host_mode.h"
#include "runtime/function/render/overlay/editor_overlay_policy.h"

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

  expect_true("editor enables overlays",
              editorOverlaysEnabled(EngineHostMode::Editor));
  expect_true("player disables overlays",
              !editorOverlaysEnabled(EngineHostMode::Player));

  // Pause is a separate flag; policy is host-mode only. Document that Pause
  // does not re-enable overlays by asserting Player stays false.
  expect_true("player still disabled (pause is orthogonal)",
              !editorOverlaysEnabled(EngineHostMode::Player));

  if (g_failures != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("editor_overlay_policy_test: all passed\n");
  return 0;
}
