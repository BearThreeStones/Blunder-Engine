#include "runtime/function/render/vulkan/vulkan_sync.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

void VulkanSync::initialize(VulkanContext* context,
                            uint32_t swapchain_image_count) {
  ASSERT(context);
  ASSERT(swapchain_image_count > 0);
  m_context = context;

  LOG_INFO("[VulkanSync::initialize] creating synchronization objects");

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
    VkResult result =
        vkCreateSemaphore(m_context->getDevice(), &semaphore_info, nullptr,
                          &m_image_available_semaphores[i]);
    if (result != VK_SUCCESS) {
      LOG_FATAL(
          "[VulkanSync::initialize] create image-available semaphore failed: "
          "{}",
          static_cast<int>(result));
    }

    result = vkCreateFence(m_context->getDevice(), &fence_info, nullptr,
                           &m_in_flight_fences[i]);
    if (result != VK_SUCCESS) {
      LOG_FATAL("[VulkanSync::initialize] create fence failed: {}",
                static_cast<int>(result));
    }
  }

  // 初始化渲染完成信号量和图像到帧的跟踪
  recreateRenderFinishedSemaphores(swapchain_image_count);
  m_images_in_flight.assign(swapchain_image_count, VK_NULL_HANDLE);
}

void VulkanSync::recreateRenderFinishedSemaphores(
    uint32_t swapchain_image_count) {
  ASSERT(m_context);
  ASSERT(swapchain_image_count > 0);

  for (VkSemaphore semaphore : m_render_finished_semaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_context->getDevice(), semaphore, nullptr);
    }
  }

  m_render_finished_semaphores.assign(swapchain_image_count, VK_NULL_HANDLE);

  // 确保图像到帧的跟踪与交换链大小匹配
  m_images_in_flight.assign(swapchain_image_count, VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (uint32_t i = 0; i < swapchain_image_count; ++i) {
    const VkResult result =
        vkCreateSemaphore(m_context->getDevice(), &semaphore_info, nullptr,
                          &m_render_finished_semaphores[i]);
    if (result != VK_SUCCESS) {
      LOG_FATAL(
          "[VulkanSync::recreateRenderFinishedSemaphores] create "
          "render-finished semaphore failed: {}",
          static_cast<int>(result));
    }
  }
}

void VulkanSync::shutdown() {
  if (!m_context) {
    return;
  }

  LOG_INFO("[VulkanSync::shutdown] destroying synchronization objects");

  vkDeviceWaitIdle(m_context->getDevice());

  for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
    if (m_image_available_semaphores[i] != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_context->getDevice(),
                         m_image_available_semaphores[i], nullptr);
      m_image_available_semaphores[i] = VK_NULL_HANDLE;
    }
    if (m_in_flight_fences[i] != VK_NULL_HANDLE) {
      vkDestroyFence(m_context->getDevice(), m_in_flight_fences[i], nullptr);
      m_in_flight_fences[i] = VK_NULL_HANDLE;
    }
  }

  for (VkSemaphore semaphore : m_render_finished_semaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_context->getDevice(), semaphore, nullptr);
    }
  }
  m_render_finished_semaphores.clear();

  for (VkFence fence : m_images_in_flight) {
    // 在 images_in_flight 中的 fences 由正在使用的帧拥有；不要在这里销毁
    (void)fence;
  }
  m_images_in_flight.clear();

  m_context = nullptr;
}

VkFence VulkanSync::getImageInFlightFence(uint32_t image_index) const {
  if (image_index < m_images_in_flight.size())
    return m_images_in_flight[image_index];
  return VK_NULL_HANDLE;
}

void VulkanSync::setImageInFlightFence(uint32_t image_index, VkFence fence) {
  if (image_index < m_images_in_flight.size())
    m_images_in_flight[image_index] = fence;
}

}  // namespace Blunder
