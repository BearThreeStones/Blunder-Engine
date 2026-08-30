#include <cstdio>

#include "runtime/function/render/render_system.h"
#include "runtime/function/render/transform_edit_viewport_notify.h"

namespace {

int g_failures = 0;

void expect_true(const char* label, bool value) {
  if (!value) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

}  // namespace

int main() {
  using namespace Blunder;

  {
    RenderSystem render;
    const uint64_t before = render.getViewportRenderGeneration();
    notifyViewportAfterInspectorLightEdit(&render, nullptr);
    expect_true("inspector light edit bumps viewport generation",
                render.getViewportRenderGeneration() == before + 1);
  }

  {
    RenderSystem render;
    const uint64_t before = render.getViewportRenderGeneration();
    notifyViewportAfterInspectorLightEdit(nullptr, nullptr);
    expect_true("inspector light notify null render is no-op",
                render.getViewportRenderGeneration() == before);
  }

  if (g_failures == 0) {
    std::printf("inspector_light_viewport_notify_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "inspector_light_viewport_notify_test: %d failure(s)\n",
               g_failures);
  return 1;
}
