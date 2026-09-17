#include "runtime/function/render/vulkan/secondary_command_buffer_pool.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

SecondaryCommandBufferPool::~SecondaryCommandBufferPool() { shutdown(); }

uint32_t SecondaryCommandBufferPool::slotIndex(SecondaryStream stream,
                                               SecondaryPass pass,
                                               uint32_t frame_index) {
  ASSERT(static_cast<uint32_t>(stream) < k_stream_count);
  ASSERT(static_cast<uint32_t>(pass) < k_pass_count);
  ASSERT(frame_index < VulkanSync::k_max_frames_in_flight);
  return (static_cast<uint32_t>(stream) * k_pass_count +
          static_cast<uint32_t>(pass)) *
             VulkanSync::k_max_frames_in_flight +
         frame_index;
}

VkSubpassContents SecondaryCommandBufferPool::toVkContents(
    rhi::SubpassContents contents) {
  return contents == rhi::SubpassContents::Secondary
             ? VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
             : VK_SUBPASS_CONTENTS_INLINE;
}

void SecondaryCommandBufferPool::initialize(VulkanContext* context) {
  shutdown();
  if (context == nullptr || context->getDevice() == VK_NULL_HANDLE) {
    return;
  }

  m_context = context;
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = context->getGraphicsQueueFamily();

  const VkResult pool_result = vkCreateCommandPool(
      context->getDevice(), &pool_info, nullptr, &m_pool);
  if (pool_result != VK_SUCCESS) {
    m_pool = VK_NULL_HANDLE;
    m_context = nullptr;
    LOG_FATAL(
        "[SecondaryCommandBufferPool::initialize] vkCreateCommandPool failed: "
        "{}",
        static_cast<int>(pool_result));
  }

  const uint32_t buffer_count =
      k_stream_count * k_pass_count * VulkanSync::k_max_frames_in_flight;
  m_buffers.resize(buffer_count, VK_NULL_HANDLE);

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = m_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  alloc_info.commandBufferCount = buffer_count;
  const VkResult alloc_result = vkAllocateCommandBuffers(
      context->getDevice(), &alloc_info, m_buffers.data());
  if (alloc_result != VK_SUCCESS) {
    vkDestroyCommandPool(context->getDevice(), m_pool, nullptr);
    m_pool = VK_NULL_HANDLE;
    m_buffers.clear();
    LOG_FATAL(
        "[SecondaryCommandBufferPool::initialize] vkAllocateCommandBuffers "
        "failed: {}",
        static_cast<int>(alloc_result));
  }
}

void SecondaryCommandBufferPool::shutdown() {
  if (m_context != nullptr && m_context->getDevice() != VK_NULL_HANDLE &&
      m_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_context->getDevice(), m_pool, nullptr);
  }
  m_pool = VK_NULL_HANDLE;
  m_buffers.clear();
  m_context = nullptr;
}

VkCommandBuffer SecondaryCommandBufferPool::get(SecondaryStream stream,
                                                SecondaryPass pass,
                                                uint32_t frame_index) const {
  if (!isAllocated()) {
    return VK_NULL_HANDLE;
  }
  const uint32_t index = slotIndex(stream, pass, frame_index);
  ASSERT(index < m_buffers.size());
  return m_buffers[index];
}

VkCommandBuffer SecondaryCommandBufferPool::begin(SecondaryStream stream,
                                                  SecondaryPass pass,
                                                  uint32_t frame_index,
                                                  VkRenderPass render_pass,
                                                  VkFramebuffer framebuffer) {
  ASSERT(isAllocated());
  ASSERT(render_pass != VK_NULL_HANDLE);
  ASSERT(framebuffer != VK_NULL_HANDLE);
  VkCommandBuffer command_buffer = get(stream, pass, frame_index);
  ASSERT(command_buffer != VK_NULL_HANDLE);

  const VkResult reset_result = vkResetCommandBuffer(command_buffer, 0);
  if (reset_result != VK_SUCCESS) {
    LOG_FATAL(
        "[SecondaryCommandBufferPool::begin] vkResetCommandBuffer failed: {}",
        static_cast<int>(reset_result));
  }

  VkCommandBufferInheritanceInfo inheritance{};
  inheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  inheritance.renderPass = render_pass;
  inheritance.subpass = 0;
  inheritance.framebuffer = framebuffer;

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
                     VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pInheritanceInfo = &inheritance;

  const VkResult begin_result = vkBeginCommandBuffer(command_buffer, &begin_info);
  if (begin_result != VK_SUCCESS) {
    LOG_FATAL(
        "[SecondaryCommandBufferPool::begin] vkBeginCommandBuffer failed: {}",
        static_cast<int>(begin_result));
  }
  return command_buffer;
}

void SecondaryCommandBufferPool::end(SecondaryStream stream, SecondaryPass pass,
                                     uint32_t frame_index) {
  VkCommandBuffer command_buffer = get(stream, pass, frame_index);
  ASSERT(command_buffer != VK_NULL_HANDLE);
  const VkResult end_result = vkEndCommandBuffer(command_buffer);
  if (end_result != VK_SUCCESS) {
    LOG_FATAL(
        "[SecondaryCommandBufferPool::end] vkEndCommandBuffer failed: {}",
        static_cast<int>(end_result));
  }
}

void SecondaryCommandBufferPool::resetFrame(SecondaryStream stream,
                                            uint32_t frame_index) {
  if (!isAllocated()) {
    return;
  }
  for (uint32_t pass = 0; pass < k_pass_count; ++pass) {
    VkCommandBuffer command_buffer =
        get(stream, static_cast<SecondaryPass>(pass), frame_index);
    if (command_buffer == VK_NULL_HANDLE) {
      continue;
    }
    vkResetCommandBuffer(command_buffer, 0);
  }
}

void SecondaryCommandBufferPool::execute(VkCommandBuffer primary,
                                         VkCommandBuffer secondary) {
  ASSERT(primary != VK_NULL_HANDLE);
  ASSERT(secondary != VK_NULL_HANDLE);
  vkCmdExecuteCommands(primary, 1, &secondary);
}

void SecondaryCommandBufferPool::execute(VkCommandBuffer primary,
                                         const VkCommandBuffer* secondaries,
                                         uint32_t count) {
  ASSERT(primary != VK_NULL_HANDLE);
  ASSERT(secondaries != nullptr);
  ASSERT(count > 0);
  vkCmdExecuteCommands(primary, count, secondaries);
}

}  // namespace Blunder
