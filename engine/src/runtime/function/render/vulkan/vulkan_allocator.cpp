#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "runtime/function/render/vulkan/vulkan_allocator.h"

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"

namespace Blunder {

void VulkanAllocator::initialize(VulkanContext* context) {
  ASSERT(context);
  m_context = context;

  LOG_INFO("[VulkanAllocator::initialize] creating VMA allocator");

  VmaAllocatorCreateInfo create_info{};
  create_info.instance = m_context->getInstance();
  create_info.physicalDevice = m_context->getPhysicalDevice();
  create_info.device = m_context->getDevice();
  create_info.vulkanApiVersion = m_context->getApiVersion();

  const VkResult result = vmaCreateAllocator(&create_info, &m_allocator);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanAllocator::initialize] vmaCreateAllocator failed: {}",
              static_cast<int>(result));
  }
}

void VulkanAllocator::shutdown() {
  LOG_INFO("[VulkanAllocator::shutdown] destroying VMA allocator");
  if (m_allocator != nullptr) {
    vmaDestroyAllocator(m_allocator);
    m_allocator = nullptr;
  }
  m_context = nullptr;
}

}  // namespace Blunder
