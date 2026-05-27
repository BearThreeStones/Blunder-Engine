#pragma once

#include <vulkan/vulkan.h>

namespace Blunder {

class OffscreenRenderTarget;
class VulkanContext;

/// Load-op render pass for screen-space overlays (navigate gizmo, HUD).
/// Runs after post-processing so SSAO composite does not clear overlay pixels.
class ScreenOverlayPass final {
 public:
  ScreenOverlayPass() = default;
  ~ScreenOverlayPass();

  void initialize(VulkanContext* ctx, OffscreenRenderTarget* offscreen);
  void shutdown();

  void begin(VkCommandBuffer cmd);
  void end(VkCommandBuffer cmd);

  VkRenderPass renderPass() const { return m_render_pass; }

 private:
  VulkanContext* m_vk_context{nullptr};
  OffscreenRenderTarget* m_offscreen_target{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
};

}  // namespace Blunder
