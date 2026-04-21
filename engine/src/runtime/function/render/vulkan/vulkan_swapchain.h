#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/vector.h"

namespace Blunder {

class VulkanContext;

class VulkanSwapchain final {
 public:
  VulkanSwapchain() = default;
  ~VulkanSwapchain() = default;

  void initialize(VulkanContext* context, uint32_t width, uint32_t height);
  void shutdown();
  void resize(uint32_t width, uint32_t height);

  VkSwapchainKHR getSwapchain() const { return m_swapchain; }
  VkFormat getImageFormat() const { return m_image_format; }
  VkExtent2D getExtent() const { return m_extent; }
  uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
  const eastl::vector<VkImage>& getImages() const { return m_images; }
  const eastl::vector<VkImageView>& getImageViews() const { return m_image_views; }

 private:
  void createSwapchain(uint32_t width, uint32_t height, VkSwapchainKHR old_swapchain);
  void createImageViews();
  void destroyImageViews();

  VulkanContext* m_context{nullptr};
  VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
  VkFormat m_image_format{VK_FORMAT_B8G8R8A8_UNORM};
  VkExtent2D m_extent{0, 0};
  eastl::vector<VkImage> m_images;
  eastl::vector<VkImageView> m_image_views;
};

}  // namespace Blunder
