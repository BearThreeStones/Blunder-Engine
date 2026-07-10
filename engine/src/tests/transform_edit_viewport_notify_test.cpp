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
    notifyViewportAfterGizmoTransformEdit(&render);
    expect_true("gizmo notify bumps viewport generation",
                render.getViewportRenderGeneration() == before + 1);
  }

  {
    RenderSystem render;
    const uint64_t before = render.getViewportRenderGeneration();
    notifyViewportAfterGizmoTransformEdit(nullptr);
    expect_true("gizmo notify null render is no-op",
                render.getViewportRenderGeneration() == before);
  }

  {
    RenderSystem render;
    const uint64_t before = render.getViewportRenderGeneration();
    // SlintSystem is optional for the unit test: null skips markViewportDirtyRegion
    // but must still force the Vulkan redraw path.
    notifyViewportAfterInspectorTransformEdit(&render, nullptr);
    expect_true("inspector notify bumps viewport generation with null slint",
                render.getViewportRenderGeneration() == before + 1);
  }

  if (g_failures == 0) {
    std::printf("transform_edit_viewport_notify_test: all passed\n");
    return 0;
  }
  std::fprintf(stderr, "transform_edit_viewport_notify_test: %d failure(s)\n",
               g_failures);
  return 1;
}
