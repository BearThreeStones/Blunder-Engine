#pragma once

#include <vulkan/vulkan.h>

#include "runtime/function/render/rhi/rhi_types.h"

namespace Blunder {

class OffscreenRenderTarget;
class OverlayLineTargets;
class VulkanContext;

/// MRT pass: CLEAR overlay_color + line_data, LOAD scene depth for line drawing.
class OverlayLinePass final {
 public:
  OverlayLinePass() = default;
  ~OverlayLinePass();

  void initialize(VulkanContext* ctx, OffscreenRenderTarget* offscreen,
                  OverlayLineTargets* targets);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  void begin(VkCommandBuffer cmd, rhi::SubpassContents contents);
  void bindViewportScissor(VkCommandBuffer cmd);
  void end(VkCommandBuffer cmd);

  VkRenderPass renderPass() const { return m_render_pass; }
  VkFramebuffer framebuffer() const;

 private:
  void createRenderPass();

  VulkanContext* m_context{nullptr};
  OffscreenRenderTarget* m_offscreen{nullptr};
  OverlayLineTargets* m_targets{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
};

}  // namespace Blunder
