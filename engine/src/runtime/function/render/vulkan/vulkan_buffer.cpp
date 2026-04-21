#include "runtime/function/render/vulkan/vulkan_buffer.h"

#include <cstddef>
#include <cstring>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"

namespace Blunder {

VkVertexInputBindingDescription Vertex::getBindingDescription() {
  VkVertexInputBindingDescription binding_description{};
  binding_description.binding = 0;
  binding_description.stride = sizeof(Vertex);
  binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return binding_description;
}

eastl::array<VkVertexInputAttributeDescription, 2>
Vertex::getAttributeDescriptions() {
  eastl::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};

  attribute_descriptions[0].binding = 0;
  attribute_descriptions[0].location = 0;
  attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attribute_descriptions[0].offset =
      static_cast<uint32_t>(offsetof(Vertex, position));

  attribute_descriptions[1].binding = 0;
  attribute_descriptions[1].location = 1;
  attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attribute_descriptions[1].offset =
      static_cast<uint32_t>(offsetof(Vertex, color));

  return attribute_descriptions;
}

void VulkanBuffer::create(VulkanAllocator* allocator, VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VmaMemoryUsage memory_usage) {
  ASSERT(allocator);
  ASSERT(size > 0);

  m_allocator = allocator;
  m_size = size;

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = size;
  buffer_info.usage = usage;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocation_info{};
  allocation_info.usage = memory_usage;

  const VkResult result = vmaCreateBuffer(
      m_allocator->getAllocator(), &buffer_info, &allocation_info, &m_buffer,
      &m_allocation, nullptr);
  if (result != VK_SUCCESS) {
    LOG_FATAL("[VulkanBuffer::create] vmaCreateBuffer failed: {}",
              static_cast<int>(result));
  }
}

void VulkanBuffer::destroy() {
  if (m_allocator && m_buffer != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE) {
    vmaDestroyBuffer(m_allocator->getAllocator(), m_buffer, m_allocation);
  }

  m_buffer = VK_NULL_HANDLE;
  m_allocation = VK_NULL_HANDLE;
  m_allocator = nullptr;
  m_size = 0;
}

void VulkanBuffer::upload(const void* data, VkDeviceSize size) {
  ASSERT(m_allocator);
  ASSERT(m_buffer != VK_NULL_HANDLE);
  ASSERT(m_allocation != VK_NULL_HANDLE);
  ASSERT(data);
  ASSERT(size > 0 && size <= m_size);

  void* mapped_data = nullptr;
  const VkResult map_result =
      vmaMapMemory(m_allocator->getAllocator(), m_allocation, &mapped_data);
  if (map_result != VK_SUCCESS) {
    LOG_FATAL("[VulkanBuffer::upload] vmaMapMemory failed: {}",
              static_cast<int>(map_result));
  }

  std::memcpy(mapped_data, data, static_cast<size_t>(size));
  vmaUnmapMemory(m_allocator->getAllocator(), m_allocation);
}

}  // namespace Blunder
