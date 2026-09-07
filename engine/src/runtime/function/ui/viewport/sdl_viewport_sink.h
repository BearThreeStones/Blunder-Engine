#pragma once

#include <cstdint>

#include <SDL3/SDL_render.h>

#include "runtime/function/ui/viewport/i_viewport_sink.h"

namespace Blunder {

class WindowSystem;

/// Player host present path: blit CPU readback frames onto the SDL window.
/// Editor keeps SlintViewportSink; Player has no Slint chrome.
class SdlViewportSink final : public IViewportSink {
 public:
  explicit SdlViewportSink(WindowSystem* window_system);
  ~SdlViewportSink() override;

  void presentViewportCpuFrame(const ViewportCpuFrame& frame) override;
  void presentViewportVulkanImage(const ViewportVulkanImage& image) override;
  void invalidateViewportPlaceholder() override;

 private:
  void destroyGpu();
  bool ensureTexture(uint32_t width, uint32_t height);

  WindowSystem* m_window_system{nullptr};
  SDL_Renderer* m_renderer{nullptr};
  SDL_Texture* m_texture{nullptr};
  uint32_t m_tex_w{0};
  uint32_t m_tex_h{0};
};

}  // namespace Blunder
