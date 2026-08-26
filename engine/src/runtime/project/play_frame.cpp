#include "runtime/project/play_frame.h"

#include "runtime/function/global/global_context.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/scene_thumbnail/scene_still.h"

namespace Blunder {

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
