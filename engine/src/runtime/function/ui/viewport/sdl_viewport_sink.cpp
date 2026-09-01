#include "runtime/function/ui/viewport/sdl_viewport_sink.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_render.h>

#include "runtime/core/base/macro.h"
#include "runtime/platform/window/window_system.h"

namespace Blunder {

SdlViewportSink::SdlViewportSink(WindowSystem* window_system)
    : m_window_system(window_system) {
  SDL_Window* window =
      window_system != nullptr ? window_system->getNativeWindow() : nullptr;
  if (window == nullptr) {
    LOG_ERROR("[SdlViewportSink] no SDL window for Player present");
    return;
  }
  m_renderer = SDL_CreateRenderer(window, nullptr);
  if (m_renderer == nullptr) {
    LOG_WARN("[SdlViewportSink] default renderer failed ({}). Trying software.",
             SDL_GetError());
    m_renderer = SDL_CreateRenderer(window, "software");
  }
  if (m_renderer == nullptr) {
    LOG_ERROR("[SdlViewportSink] SDL_CreateRenderer failed: {}", SDL_GetError());
  } else {
    (void)SDL_SetRenderVSync(m_renderer, 1);
  }
}

SdlViewportSink::~SdlViewportSink() { destroyGpu(); }

void SdlViewportSink::destroyGpu() {
  if (m_texture != nullptr) {
    SDL_DestroyTexture(m_texture);
    m_texture = nullptr;
  }
  m_tex_w = 0;
  m_tex_h = 0;
  if (m_renderer != nullptr) {
    SDL_DestroyRenderer(m_renderer);
    m_renderer = nullptr;
  }
}

bool SdlViewportSink::ensureTexture(uint32_t width, uint32_t height) {
  if (m_renderer == nullptr || width == 0 || height == 0) {
    return false;
  }
  if (m_texture != nullptr && m_tex_w == width && m_tex_h == height) {
    return true;
  }
  if (m_texture != nullptr) {
    SDL_DestroyTexture(m_texture);
    m_texture = nullptr;
  }
  m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING,
                                static_cast<int>(width),
                                static_cast<int>(height));
  if (m_texture == nullptr) {
    LOG_ERROR("[SdlViewportSink] SDL_CreateTexture failed: {}", SDL_GetError());
    m_tex_w = 0;
    m_tex_h = 0;
    return false;
  }
  m_tex_w = width;
  m_tex_h = height;
  return true;
}

void SdlViewportSink::presentViewportCpuFrame(const ViewportCpuFrame& frame) {
  if (m_renderer == nullptr || frame.pixels_rgba == nullptr ||
      frame.width == 0 || frame.height == 0) {
    return;
  }
  if (!ensureTexture(frame.width, frame.height)) {
    return;
  }
  const int pitch = frame.stride_bytes != 0
                        ? static_cast<int>(frame.stride_bytes)
                        : static_cast<int>(frame.width * 4u);
  if (!SDL_UpdateTexture(m_texture, nullptr, frame.pixels_rgba, pitch)) {
    LOG_ERROR("[SdlViewportSink] SDL_UpdateTexture failed: {}", SDL_GetError());
    return;
  }
  (void)SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
  (void)SDL_RenderClear(m_renderer);
  if (!SDL_RenderTexture(m_renderer, m_texture, nullptr, nullptr)) {
    LOG_ERROR("[SdlViewportSink] SDL_RenderTexture failed: {}", SDL_GetError());
    return;
  }
  if (!SDL_RenderPresent(m_renderer)) {
    LOG_ERROR("[SdlViewportSink] SDL_RenderPresent failed: {}", SDL_GetError());
    return;
  }
}

void SdlViewportSink::presentViewportVulkanImage(
    const ViewportVulkanImage& image) {
  (void)image;
}

void SdlViewportSink::invalidateViewportPlaceholder() {}

}  // namespace Blunder
