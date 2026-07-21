#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_context.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"

// Prefer logging leftover allocations over aborting the process. Player/editor
// teardown can still leave transient VMA blocks; crashing on close blocks
// Play session Stop. OS reclaims GPU memory on process exit.
#ifndef VMA_ASSERT
#define VMA_ASSERT(expr)                                                       \
  do {                                                                         \
    if (!(expr)) {                                                             \
      LOG_ERROR("[VMA] assert failed: {}", #expr);                             \
    }                                                                          \
  } while (0)
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

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
