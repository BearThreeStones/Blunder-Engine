#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Blunder {

class VulkanAllocator;
class VulkanContext;

/// Depth-only shadow map render target (directional light).
class ShadowMapTarget final {
 public:
  static constexpr uint32_t k_shadow_map_size = 1024;

  ShadowMapTarget() = default;
  ~ShadowMapTarget() = default;

  void initialize(VulkanContext* context, VulkanAllocator* allocator);
  void shutdown();

  void beginRenderPass(VkCommandBuffer cmd, float depth_clear = 1.0f);
  void endRenderPass(VkCommandBuffer cmd);
  void cmdBarrierToShaderReadDepth(VkCommandBuffer cmd);

  VkRenderPass getRenderPass() const { return m_render_pass; }
  VkFramebuffer getFramebuffer() const { return m_framebuffer; }
  VkImageView getDepthImageView() const { return m_depth_image_view; }
  VkSampler getComparisonSampler() const { return m_comparison_sampler; }
  VkExtent2D getExtent() const {
    return {k_shadow_map_size, k_shadow_map_size};
  }

 private:
  void createRenderPass();
  void createResources();
  void destroyResources();

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  VkImage m_depth_image{VK_NULL_HANDLE};
  VmaAllocation m_depth_allocation{VK_NULL_HANDLE};
  VkImageView m_depth_image_view{VK_NULL_HANDLE};
  VkSampler m_comparison_sampler{VK_NULL_HANDLE};
  VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
  VkImageLayout m_current_layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

}  // namespace Blunder
