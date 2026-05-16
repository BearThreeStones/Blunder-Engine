#include "runtime/function/render/vulkan_backend/vulkan_gpu_buffer.h"

#include <vulkan/vulkan.h>

#include "runtime/function/render/vulkan/vulkan_buffer.h"

namespace Blunder::vulkan_backend {

void VulkanGpuBuffer::setBuffer(eastl::unique_ptr<VulkanBuffer> buffer) {
  m_buffer = eastl::move(buffer);
}

void VulkanGpuBuffer::upload(const void* data, uint64_t size) {
  if (m_buffer) {
    m_buffer->upload(data, static_cast<VkDeviceSize>(size));
  }
}

uint64_t VulkanGpuBuffer::size() const {
  return m_buffer ? static_cast<uint64_t>(m_buffer->getSize()) : 0;
}

}  // namespace Blunder::vulkan_backend
