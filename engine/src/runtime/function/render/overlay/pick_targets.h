#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Blunder {

class VulkanAllocator;
class VulkanContext;

/// Offscreen targets for viewport entity picking (entity ID + depth).
class PickTargets final {
 public:
  PickTargets() = default;
  ~PickTargets();

  void initialize(VulkanContext* ctx, VulkanAllocator* alloc);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  VkImage entityIdImage() const { return m_entity_id_image; }
  VkImageView entityIdView() const { return m_entity_id_view; }
  VkImageLayout entityIdLayout() const { return m_entity_id_layout; }
  void setEntityIdLayout(VkImageLayout layout) { m_entity_id_layout = layout; }

  VkImage pickDepthImage() const { return m_pick_depth_image; }
  VkImageLayout pickDepthLayout() const { return m_pick_depth_layout; }
  void setPickDepthLayout(VkImageLayout layout) { m_pick_depth_layout = layout; }

  VkRenderPass prepassRenderPass() const { return m_prepass_render_pass; }
  VkFramebuffer prepassFramebuffer() const { return m_prepass_framebuffer; }
  VkExtent2D extent() const;

  void cmdBarrierEntityIdToTransferSrc(VkCommandBuffer cmd);
  void cmdBarrierDepthToTransferSrc(VkCommandBuffer cmd);
  void cmdPrepareForPrepassRender(VkCommandBuffer cmd);

 private:
  void destroyResources();
  void createPrepassRenderPass();
  void destroyPrepassRenderPass();
  void recreatePrepassFramebuffer();

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};

  uint32_t m_width{0};
  uint32_t m_height{0};

  VkImage m_entity_id_image{VK_NULL_HANDLE};
  VmaAllocation m_entity_id_allocation{VK_NULL_HANDLE};
  VkImageView m_entity_id_view{VK_NULL_HANDLE};
  VkImageLayout m_entity_id_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkImage m_pick_depth_image{VK_NULL_HANDLE};
  VmaAllocation m_pick_depth_allocation{VK_NULL_HANDLE};
  VkImageView m_pick_depth_view{VK_NULL_HANDLE};
  VkImageLayout m_pick_depth_layout{VK_IMAGE_LAYOUT_UNDEFINED};

  VkRenderPass m_prepass_render_pass{VK_NULL_HANDLE};
  VkFramebuffer m_prepass_framebuffer{VK_NULL_HANDLE};
};

}  // namespace Blunder
