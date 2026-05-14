#pragma once

#include <vulkan/vulkan.h>

#include "runtime/function/render/vulkan/vulkan_image.h"

namespace Blunder {

class Texture2DAsset;
class VulkanAllocator;
class VulkanContext;

class VulkanTexture final {
 public:
  VulkanTexture() = default;
  ~VulkanTexture() = default;

  void createFromTexture2DAsset(VulkanContext* context,
                                VulkanAllocator* allocator,
                                const Texture2DAsset& asset);
  void destroy();

  const VulkanImage& getImage() const { return m_image; }
  VkImageView getImageView() const { return m_image.getImageView(); }
  VkSampler getSampler() const { return m_image.getSampler(); }
  VkExtent2D getExtent() const { return m_image.getExtent(); }
  VkFormat getFormat() const { return m_image.getFormat(); }
  VkImageLayout getCurrentLayout() const { return m_image.getCurrentLayout(); }

 private:
  VulkanContext* m_context{nullptr};
  VulkanAllocator* m_allocator{nullptr};
  VulkanImage m_image;
};

}  // namespace Blunder