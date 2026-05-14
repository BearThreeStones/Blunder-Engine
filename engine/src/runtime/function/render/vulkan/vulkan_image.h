#pragma once

#include <cstddef>
#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Blunder {

class Texture2DAsset;
class VulkanAllocator;
class VulkanContext;

class VulkanImage final {
 public:
  VulkanImage() = default;
  ~VulkanImage() = default;

  void create(VulkanContext* context, VulkanAllocator* allocator,
              uint32_t width, uint32_t height, VkFormat format,
              VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT,
              VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);
  void destroy();

  void uploadTexture2D(const Texture2DAsset& texture);
  void cmdTransitionLayout(VkCommandBuffer command_buffer,
                           VkImageLayout new_layout);

  VkImage getImage() const { return m_image; }
  VkImageView getImageView() const { return m_image_view; }
  VkSampler getSampler() const { return m_sampler; }
  VmaAllocation getAllocation() const { return m_allocation; }
  VkExtent2D getExtent() const { return {m_width, m_height}; }
  VkFormat getFormat() const { return m_format; }
  VkImageLayout getCurrentLayout() const { return m_current_layout; }
  void setCurrentLayout(VkImageLayout layout) { m_current_layout = layout; }
  uint32_t getWidth() const { return m_width; }
  uint32_t getHeight() const { return m_height; }
  bool isValid() const { return m_image != VK_NULL_HANDLE; }

 private:
  void createImageView();
  void createSampler();
  void uploadPixels(const void* pixel_data, size_t pixel_byte_size,
                    uint32_t width, uint32_t height);

  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  VkImage m_image{VK_NULL_HANDLE};
  VmaAllocation m_allocation{VK_NULL_HANDLE};
  VkImageView m_image_view{VK_NULL_HANDLE};
  VkSampler m_sampler{VK_NULL_HANDLE};
  uint32_t m_width{0};
  uint32_t m_height{0};
  VkFormat m_format{VK_FORMAT_UNDEFINED};
  VkImageUsageFlags m_usage{0};
  VkImageAspectFlags m_aspect_mask{VK_IMAGE_ASPECT_COLOR_BIT};
  VkImageLayout m_current_layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

}  // namespace Blunder