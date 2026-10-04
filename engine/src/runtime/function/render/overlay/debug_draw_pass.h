#pragma once

#include <vulkan/vulkan.h>

#include "runtime/function/render/rhi/rhi_types.h"

namespace Blunder {

class OffscreenRenderTarget;
class VulkanContext;

/// LOAD color + scene depth. Depth-tested debug lines (x-ray). Not ScreenOverlayPass.
class DebugDrawPass final {
 public:
  DebugDrawPass() = default;
  ~DebugDrawPass();

  void initialize(VulkanContext* ctx, OffscreenRenderTarget* offscreen);
  void shutdown();

  void begin(VkCommandBuffer cmd, rhi::SubpassContents contents);
  void bindViewportScissor(VkCommandBuffer cmd, uint32_t width, uint32_t height);
  void end(VkCommandBuffer cmd);

  /// Offscreen LOAD pass: color + scene depth. Compatible with the scene
  /// framebuffer. Not ScreenOverlayPass.
  VkRenderPass renderPass() const;

 private:
  VulkanContext* m_vk_context{nullptr};
  OffscreenRenderTarget* m_offscreen_target{nullptr};
};

}  // namespace Blunder
