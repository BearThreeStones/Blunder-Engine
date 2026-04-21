#pragma once

#include <vulkan/vulkan.h>

#include <glm/vec3.hpp>
#include <vk_mem_alloc.h>

#include "EASTL/array.h"

namespace Blunder {

class VulkanAllocator;

struct Vertex {
  glm::vec3 position;
  glm::vec3 color;

  static VkVertexInputBindingDescription getBindingDescription();
  static eastl::array<VkVertexInputAttributeDescription, 2>
  getAttributeDescriptions();
};

class VulkanBuffer final {
 public:
  VulkanBuffer() = default;
  ~VulkanBuffer() = default;

  void create(VulkanAllocator* allocator, VkDeviceSize size,
              VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
  void destroy();
  void upload(const void* data, VkDeviceSize size);

  VkBuffer getBuffer() const { return m_buffer; }

 private:
  VulkanAllocator* m_allocator{nullptr};
  VkBuffer m_buffer{VK_NULL_HANDLE};
  VmaAllocation m_allocation{VK_NULL_HANDLE};
  VkDeviceSize m_size{0};
};

}  // namespace Blunder
