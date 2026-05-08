#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Blunder {

class VulkanAllocator;
class VulkanContext;

/// Off-screen render target used by the 3D scene pass so its output can be
/// sampled by another pass (e.g. UI overlay) or read back to CPU and uploaded
/// to a Slint Image control.
///
/// The render pass owned by this target outputs the color attachment in the
/// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout, ready for sampling.
/// Use cmdBarrierToTransferSrc() to transition it for vkCmdCopyImageToBuffer
/// when reading back to CPU.
class OffscreenRenderTarget final {
 public:
  OffscreenRenderTarget() = default;
  ~OffscreenRenderTarget() = default;

  void initialize(VulkanContext* context, VulkanAllocator* allocator,
                  uint32_t width, uint32_t height,
                  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  VkRenderPass getRenderPass() const { return m_render_pass; }
  VkImage getImage() const { return m_image; }
  VkImageView getImageView() const { return m_image_view; }
  VkFramebuffer getFramebuffer() const { return m_framebuffer; }
  VkExtent2D getExtent() const { return {m_width, m_height}; }
  VkFormat getFormat() const { return m_format; }

  /// Layout right after the offscreen render pass finishes:
  /// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
  VkImageLayout getCurrentLayout() const { return m_current_layout; }
  void setCurrentLayout(VkImageLayout layout) { m_current_layout = layout; }

  /// Transition the color image from SHADER_READ_ONLY_OPTIMAL to
  /// TRANSFER_SRC_OPTIMAL so it can be copied with vkCmdCopyImageToBuffer.
  void cmdBarrierToTransferSrc(VkCommandBuffer cmd);
  /// Transition back to SHADER_READ_ONLY_OPTIMAL after the copy completes,
  /// so subsequent passes can sample the image again.
  void cmdBarrierToShaderRead(VkCommandBuffer cmd);

 private:
  void createRenderPass();
  void createImageAndFramebuffer();
  void destroyImageAndFramebuffer();

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  VkRenderPass m_render_pass{VK_NULL_HANDLE};
  VkImage m_image{VK_NULL_HANDLE};
  VmaAllocation m_image_allocation{VK_NULL_HANDLE};
  VkImageView m_image_view{VK_NULL_HANDLE};
  VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
  uint32_t m_width{0};
  uint32_t m_height{0};
  VkFormat m_format{VK_FORMAT_R8G8B8A8_UNORM};
  VkImageLayout m_current_layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

}  // namespace Blunder
