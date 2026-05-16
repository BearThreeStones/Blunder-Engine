#include "runtime/function/render/vulkan_backend/vulkan_command_list.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder::vulkan_backend {

void VulkanCommandList::bind(VulkanContext* context,
                             VkCommandBuffer command_buffer) {
  m_context = context;
  m_command_buffer = command_buffer;
}

void VulkanCommandList::begin() {
  ASSERT(m_context && m_command_buffer != VK_NULL_HANDLE);
  vkResetCommandBuffer(m_command_buffer, 0);
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  const VkResult result =
      vkBeginCommandBuffer(m_command_buffer, &begin_info);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanCommandList::begin] vkBeginCommandBuffer failed: {}",
              static_cast<int>(result));
  }
}

void VulkanCommandList::end() {
  ASSERT(m_command_buffer != VK_NULL_HANDLE);
  const VkResult result = vkEndCommandBuffer(m_command_buffer);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanCommandList::end] vkEndCommandBuffer failed: {}",
              static_cast<int>(result));
  }
}

void VulkanCommandList::submit() {
  // Queue submit is handled by RenderSystem via VulkanSync / VulkanContext.
}

}  // namespace Blunder::vulkan_backend
