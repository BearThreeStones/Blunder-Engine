#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Blunder {

class OffscreenRenderTarget;
class VulkanAllocator;
class VulkanContext;

/// Offscreen targets for selection outline prepass (object ID + depth).
class OutlineTargets final {
 public:
  OutlineTargets() = default;
  ~OutlineTargets();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  VkFormat objectIdFormat() const { return m_object_id_format; }
  VkImageView objectIdView() const { return m_object_id_view; }
  VkImageView outlineDepthView() const { return m_outline_depth_view; }
  VkRenderPass prepassRenderPass() const { return m_prepass_render_pass; }
  VkFramebuffer prepassFramebuffer() const { return m_prepass_framebuffer; }
  VkExtent2D extent() const;

  void cmdBarrierToShaderRead(VkCommandBuffer cmd);

 private:
  void destroyResources();
  void createPrepassRenderPass();
  void destroyPrepassRenderPass();
  void recreatePrepassFramebuffer();
  static VkFormat pickObjectIdFormat(VkPhysicalDevice physical_device);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};
  VkFormat m_object_id_format{VK_FORMAT_R16_UINT};

  VkImage m_object_id_image{VK_NULL_HANDLE};
  VmaAllocation m_object_id_allocation{VK_NULL_HANDLE};
  VkImageView m_object_id_view{VK_NULL_HANDLE};
  VkImageLayout m_object_id_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkImage m_outline_depth_image{VK_NULL_HANDLE};
  VmaAllocation m_outline_depth_allocation{VK_NULL_HANDLE};
  VkImageView m_outline_depth_view{VK_NULL_HANDLE};
  VkImageLayout m_outline_depth_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkRenderPass m_prepass_render_pass{VK_NULL_HANDLE};
  VkFramebuffer m_prepass_framebuffer{VK_NULL_HANDLE};
};

}  // namespace Blunder
