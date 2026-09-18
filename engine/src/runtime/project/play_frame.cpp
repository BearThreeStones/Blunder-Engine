#include "runtime/project/play_frame.h"

#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"

#include <chrono>

namespace Blunder {

void waitUntilTextureUploadsIdle(uint32_t timeout_ms,
                                 const std::function<void()>& pump) {
  auto* render = g_runtime_global_context.m_render_system.get();
  if (render == nullptr) {
    return;
  }
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);
  while (render->textureUploadInFlightCount() > 0u &&
         std::chrono::steady_clock::now() < deadline) {
    if (pump) {
      pump();
    }
    render->requestViewportRedraw();
  }
  for (int i = 0; i < 4; ++i) {
    if (pump) {
      pump();
    }
    render->requestViewportRedraw();
  }
}

bool capturePlayProcessFrame(eastl::vector<uint8_t>& out_rgba, uint32_t& out_width,
                             uint32_t& out_height) {
  out_rgba.clear();
  out_width = 0;
  out_height = 0;
  if (!g_runtime_global_context.m_render_system) {
    return false;
  }
  eastl::vector<uint8_t> src;
  uint32_t src_w = 0;
  uint32_t src_h = 0;
  if (!g_runtime_global_context.m_render_system->readbackOffscreenRgba(src, src_w,
                                                                      src_h)) {
    return false;
  }
  SceneStillExtent extent{};
  if (!fitRgbaToSceneStill(src.data(), src_w, src_h, k_capture_aspect_w,
                           k_capture_aspect_h, k_capture_longest_edge, out_rgba,
                           extent)) {
    return false;
  }
  out_width = extent.width;
  out_height = extent.height;
  return true;
}

}  // namespace Blunder
