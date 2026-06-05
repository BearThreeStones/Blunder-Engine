#pragma once

#include <cstdint>

#include "EASTL/array.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Blunder {

class VulkanAllocator;
class VulkanContext;

/// Off-screen render target used by the 3D scene pass so its output can be
/// sampled by another pass (e.g. UI overlay) or read back to CPU and uploaded
/// to a Slint Image control.
///
/// Color and depth attachments are double-buffered (one set per in-flight frame)
/// so the GPU can write buffer[slot] while Slint samples a completed buffer.
class OffscreenRenderTarget final {
 public:
  static constexpr uint32_t k_buffer_count = 2;

  OffscreenRenderTarget() = default;
  ~OffscreenRenderTarget() = default;

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                  uint32_t width, uint32_t height,
                  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  void setActiveBufferIndex(uint32_t index);
  uint32_t getActiveBufferIndex() const { return m_active_buffer_index; }

  VkRenderPass getRenderPass() const { return m_render_pass; }
  VkImage getImage() const;
  VkImage getImage(uint32_t buffer_index) const;
  VkImageView getImageView() const;
  VkImage getDepthImage() const;
  VkImageView getDepthImageView() const;
  VkFramebuffer getFramebuffer() const;
  VkExtent2D getExtent() const { return {m_width, m_height}; }
  VkFormat getFormat() const { return m_format; }

  VkImageLayout getCurrentLayout() const;
  void setCurrentLayout(VkImageLayout layout);
  VkImageLayout getDepthLayout() const;
  void setDepthLayout(VkImageLayout layout);

  void cmdBarrierToTransferSrc(VkCommandBuffer cmd);
  void cmdBarrierToShaderRead(VkCommandBuffer cmd);
  void cmdBarrierDepthToShaderRead(VkCommandBuffer cmd);

 private:
  struct BufferSlot {
    VkImage color_image{VK_NULL_HANDLE};
    VmaAllocation color_allocation{VK_NULL_HANDLE};
    VkImageView color_view{VK_NULL_HANDLE};
    VkImage depth_image{VK_NULL_HANDLE};
    VmaAllocation depth_allocation{VK_NULL_HANDLE};
    VkImageView depth_view{VK_NULL_HANDLE};
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    VkImageLayout color_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout depth_layout{VK_IMAGE_LAYOUT_UNDEFINED};
  };

  void createRenderPass();
  void createImageAndFramebuffer();
  void destroyImageAndFramebuffer();
  void createBufferSlot(uint32_t slot_index);
  void destroyBufferSlot(uint32_t slot_index);

  const BufferSlot& activeSlot() const;
  BufferSlot& activeSlot();

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  eastl::array<BufferSlot, k_buffer_count> m_buffers{};
  uint32_t m_active_buffer_index{0};
  uint32_t m_width{0};
  uint32_t m_height{0};
  VkFormat m_format{VK_FORMAT_R8G8B8A8_UNORM};
};

}  // namespace Blunder
