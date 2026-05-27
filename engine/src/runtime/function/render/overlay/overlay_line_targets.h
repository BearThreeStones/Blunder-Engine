#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Blunder {

class OffscreenRenderTarget;
class VulkanAllocator;
class VulkanContext;

/// Offscreen MRT targets for overlay line anti-aliasing (overlay color + line data).
class OverlayLineTargets final {
 public:
  OverlayLineTargets() = default;
  ~OverlayLineTargets();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc,
                    OffscreenRenderTarget* offscreen);
  void shutdown();
  void resize(uint32_t width, uint32_t height);
  void recreateFramebuffer(VkRenderPass render_pass);

  VkImageView overlayColorView() const { return m_overlay_color_view; }
  VkImageView lineDataView() const { return m_line_data_view; }
  VkFramebuffer framebuffer() const { return m_framebuffer; }
  VkExtent2D extent() const;

  void cmdBarrierOverlayTargetsToShaderRead(VkCommandBuffer cmd);

 private:
  void destroyImages();

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  OffscreenRenderTarget* m_offscreen{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};

  VkImage m_overlay_color_image{VK_NULL_HANDLE};
  VmaAllocation m_overlay_color_allocation{VK_NULL_HANDLE};
  VkImageView m_overlay_color_view{VK_NULL_HANDLE};
  VkImageLayout m_overlay_color_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkImage m_line_data_image{VK_NULL_HANDLE};
  VmaAllocation m_line_data_allocation{VK_NULL_HANDLE};
  VkImageView m_line_data_view{VK_NULL_HANDLE};
  VkImageLayout m_line_data_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
};

}  // namespace Blunder
