#include "runtime/function/render/vulkan/vulkan_sync.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

void VulkanSync::initialize(VulkanContext* context,
                            uint32_t swapchain_image_count) {
  shutdown();
  m_context = context;
  m_next_timeline_value = 0;
  m_slot_values = {0, 0};

  if (m_context == nullptr || m_context->getDevice() == VK_NULL_HANDLE) {
    return;
  }

  ASSERT(swapchain_image_count > 0);

  LOG_INFO("[VulkanSync::initialize] creating synchronization objects");

  VkSemaphoreTypeCreateInfo timeline_type{};
  timeline_type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  timeline_type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  timeline_type.initialValue = 0;

  VkSemaphoreCreateInfo timeline_info{};
  timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  timeline_info.pNext = &timeline_type;
  const VkResult timeline_result = vkCreateSemaphore(
      m_context->getDevice(), &timeline_info, nullptr, &m_timeline);
  if (timeline_result != VK_SUCCESS) {
    m_timeline = VK_NULL_HANDLE;
    LOG_FATAL("[VulkanSync::initialize] create timeline semaphore failed: {}",
              static_cast<int>(timeline_result));
  }

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
    const VkResult result =
        vkCreateSemaphore(m_context->getDevice(), &semaphore_info, nullptr,
                          &m_image_available_semaphores[i]);
    if (result != VK_SUCCESS) {
      LOG_FATAL(
          "[VulkanSync::initialize] create image-available semaphore failed: "
          "{}",
          static_cast<int>(result));
    }
  }

  recreateRenderFinishedSemaphores(swapchain_image_count);
  m_images_in_flight.assign(swapchain_image_count, VK_NULL_HANDLE);
}

void VulkanSync::recreateRenderFinishedSemaphores(
    uint32_t swapchain_image_count) {
  if (m_context == nullptr || m_context->getDevice() == VK_NULL_HANDLE) {
    return;
  }
  ASSERT(swapchain_image_count > 0);

  for (VkSemaphore semaphore : m_render_finished_semaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_context->getDevice(), semaphore, nullptr);
    }
  }

  m_render_finished_semaphores.assign(swapchain_image_count, VK_NULL_HANDLE);
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
  if (m_context != nullptr && m_context->getDevice() != VK_NULL_HANDLE) {
    LOG_INFO("[VulkanSync::shutdown] destroying synchronization objects");
    vkDeviceWaitIdle(m_context->getDevice());

    if (m_timeline != VK_NULL_HANDLE) {
      vkDestroySemaphore(m_context->getDevice(), m_timeline, nullptr);
      m_timeline = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
      if (m_image_available_semaphores[i] != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_context->getDevice(),
                           m_image_available_semaphores[i], nullptr);
        m_image_available_semaphores[i] = VK_NULL_HANDLE;
      }
    }

    for (VkSemaphore semaphore : m_render_finished_semaphores) {
      if (semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_context->getDevice(), semaphore, nullptr);
      }
    }
  }

  m_render_finished_semaphores.clear();
  m_images_in_flight.clear();
  m_next_timeline_value = 0;
  m_slot_values = {0, 0};
  m_timeline = VK_NULL_HANDLE;
  m_context = nullptr;
}

uint64_t VulkanSync::issueValue() { return ++m_next_timeline_value; }

uint64_t VulkanSync::queueSubmit(VkQueue queue, const VkSubmitInfo& base_info) {
  ASSERT(m_context);
  ASSERT(m_context->getDevice() != VK_NULL_HANDLE);
  ASSERT(queue != VK_NULL_HANDLE);
  if (m_timeline == VK_NULL_HANDLE) {
    LOG_FATAL("[VulkanSync::queueSubmit] timeline semaphore is not allocated");
  }

  const uint64_t value = issueValue();
  VkSemaphore signal_semaphore = m_timeline;

  VkTimelineSemaphoreSubmitInfo timeline_info{};
  timeline_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timeline_info.pNext = base_info.pNext;
  timeline_info.signalSemaphoreValueCount = 1;
  timeline_info.pSignalSemaphoreValues = &value;

  VkSubmitInfo submit_info = base_info;
  submit_info.pNext = &timeline_info;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &signal_semaphore;

  const VkResult submit_result =
      vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
  if (submit_result != VK_SUCCESS) {
    LOG_FATAL("[VulkanSync::queueSubmit] vkQueueSubmit failed: {}",
              static_cast<int>(submit_result));
  }
  return value;
}

void VulkanSync::setSlotValue(uint32_t slot, uint64_t value) {
  if (slot < k_max_frames_in_flight) {
    m_slot_values[slot] = value;
  }
}

uint64_t VulkanSync::slotValue(uint32_t slot) const {
  if (slot >= k_max_frames_in_flight) {
    return 0;
  }
  return m_slot_values[slot];
}

bool VulkanSync::slotReached(uint32_t slot) const {
  if (slot >= k_max_frames_in_flight) {
    return false;
  }
  return valueReached(m_slot_values[slot]);
}

bool VulkanSync::valueReached(uint64_t value) const {
  // Poll via vkWaitSemaphores(timeout=0). Unlike GetSemaphoreCounterValue,
  // success makes the signal visible to the host.
  return waitValue(value, 0) == VK_SUCCESS;
}

VkResult VulkanSync::waitValue(uint64_t value, uint64_t timeout_ns) const {
  if (value == 0 || m_timeline == VK_NULL_HANDLE || m_context == nullptr ||
      m_context->getDevice() == VK_NULL_HANDLE) {
    return VK_SUCCESS;
  }
  VkSemaphore semaphore = m_timeline;
  VkSemaphoreWaitInfo wait_info{};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1;
  wait_info.pSemaphores = &semaphore;
  wait_info.pValues = &value;
  return vkWaitSemaphores(m_context->getDevice(), &wait_info, timeout_ns);
}

VkResult VulkanSync::waitSlot(uint32_t slot, uint64_t timeout_ns) const {
  if (slot >= k_max_frames_in_flight) {
    return VK_ERROR_UNKNOWN;
  }
  return waitValue(m_slot_values[slot], timeout_ns);
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
