#include "runtime/function/render/vulkan/vulkan_swapchain.h"

#include "EASTL/algorithm.h"
#include <limits>
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

namespace {

struct SwapchainSupportDetails {
  // 基本表面能力（交换链中图像的最小/最大数量、图像的最小/最大宽度和高度）
  VkSurfaceCapabilitiesKHR capabilities{};
  // 表面格式（像素格式，色彩空间）
  eastl::vector<VkSurfaceFormatKHR> formats;
  // 可用的呈现模式
  eastl::vector<VkPresentModeKHR> present_modes;
};

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physical_device,
                                              VkSurfaceKHR surface) {
  SwapchainSupportDetails details;

  // 查询基本表面能力
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                            &details.capabilities);

  // 查询受支持的表面格式
  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                       nullptr);
  if (format_count > 0) {
    details.formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                         details.formats.data());
  }

  // 查询受支持的呈现模式的方式与使用
  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_mode_count, nullptr);
  if (present_mode_count > 0) {
    details.present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count,
        details.present_modes.data());
  }

  return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const eastl::vector<VkSurfaceFormatKHR>& available_formats) {
  for (const VkSurfaceFormatKHR& format : available_formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }

  ASSERT(!available_formats.empty());
  return available_formats[0];
}

VkPresentModeKHR chooseSwapPresentMode(
    const eastl::vector<VkPresentModeKHR>& available_present_modes) {
  for (const VkPresentModeKHR mode : available_present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                        uint32_t width, uint32_t height) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  VkExtent2D actual_extent{width, height};
  actual_extent.width =
      std::clamp(actual_extent.width, capabilities.minImageExtent.width,
                 capabilities.maxImageExtent.width);
  actual_extent.height =
      std::clamp(actual_extent.height, capabilities.minImageExtent.height,
                 capabilities.maxImageExtent.height);
  return actual_extent;
}

}  // namespace

void VulkanSwapchain::initialize(VulkanContext* context, uint32_t width,
                                 uint32_t height) {
  ASSERT(context);
  m_context = context;
  createSwapchain(width, height, VK_NULL_HANDLE);
  createImageViews();
  LOG_INFO("[VulkanSwapchain::initialize] created {}x{}", width, height);
}

void VulkanSwapchain::shutdown() {
  if (!m_context) {
    return;
  }

  LOG_INFO("[VulkanSwapchain::shutdown] destroying swapchain resources");

  vkDeviceWaitIdle(m_context->getDevice());
  destroyImageViews();

  if (m_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(m_context->getDevice(), m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }

  m_images.clear();
  m_context = nullptr;
}

void VulkanSwapchain::resize(uint32_t width, uint32_t height) {
  ASSERT(m_context);
  if (width == 0 || height == 0) {
    return;
  }

  LOG_INFO("[VulkanSwapchain::resize] recreating {}x{}", width, height);

  vkDeviceWaitIdle(m_context->getDevice());

  VkSwapchainKHR old_swapchain = m_swapchain;
  destroyImageViews();
  createSwapchain(width, height, old_swapchain);
  createImageViews();

  if (old_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(m_context->getDevice(), old_swapchain, nullptr);
  }
}

void VulkanSwapchain::createSwapchain(uint32_t width, uint32_t height,
                                      VkSwapchainKHR old_swapchain) {
  SwapchainSupportDetails support = querySwapchainSupport(
      m_context->getPhysicalDevice(), m_context->getSurface());

  // 至少存在一种受支持的图像格式和一种受支持的呈现模式，视为足够交换链支持
  if (support.formats.empty() || support.present_modes.empty()) {
    LOG_FATAL("[VulkanSwapchain::createSwapchain] swapchain unsupported");
  }

  VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(support.formats);
  VkPresentModeKHR present_mode = chooseSwapPresentMode(support.present_modes);
  VkExtent2D extent = chooseSwapExtent(support.capabilities, width, height);

  // 比最小值多一个图像，以允许在显示一张图像的同时准备下一帧
  uint32_t image_count = support.capabilities.minImageCount + 1;
  if (support.capabilities.maxImageCount > 0 &&
      image_count > support.capabilities.maxImageCount) {
    image_count = support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = m_context->getSurface();
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  // 暂时为 1
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queue_family_indices[] = {m_context->getGraphicsQueueFamily(),
                                     m_context->getPresentQueueFamily()};
  // 如果图形队列和呈现队列属于不同的队列族，则需要使用并发共享模式
  if (m_context->getGraphicsQueueFamily() != m_context->getPresentQueueFamily()) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indices;
    // 否则，使用独占模式（可能提供更高的性能）
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // 可选的
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
  }

  create_info.preTransform = support.capabilities.currentTransform;

  // 不使用 alpha 通道与窗口系统中的其他窗口进行混合
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  // 开启裁剪功能，允许某些平台优化显示性能（例如，隐藏窗口部分时不渲染这些部分）
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = old_swapchain;

  const VkResult result = vkCreateSwapchainKHR(
      m_context->getDevice(), &create_info, nullptr, &m_swapchain);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanSwapchain::createSwapchain] vkCreateSwapchainKHR failed: {}",
              static_cast<int>(result));
  }

  vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapchain, &image_count,
                          nullptr);
  m_images.resize(image_count);
  vkGetSwapchainImagesKHR(m_context->getDevice(), m_swapchain, &image_count,
                          m_images.data());

  m_image_format = surface_format.format;
  m_extent = extent;
}

void VulkanSwapchain::createImageViews() {
  ASSERT(m_context);

  m_image_views.resize(m_images.size());
  for (size_t i = 0; i < m_images.size(); ++i) {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = m_images[i];
    // 二维纹理
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = m_image_format;
    // 使用默认映射
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    const VkResult result = vkCreateImageView(m_context->getDevice(), &create_info,
                                              nullptr, &m_image_views[i]);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[VulkanSwapchain::createImageViews] vkCreateImageView failed: {}",
                static_cast<int>(result));
    }
  }
}

void VulkanSwapchain::destroyImageViews() {
  if (!m_context) {
    return;
  }

  for (VkImageView image_view : m_image_views) {
    if (image_view != VK_NULL_HANDLE) {
      vkDestroyImageView(m_context->getDevice(), image_view, nullptr);
    }
  }
  m_image_views.clear();
}

}  // namespace Blunder
