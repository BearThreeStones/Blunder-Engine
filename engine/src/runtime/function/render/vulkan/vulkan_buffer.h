#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "EASTL/array.h"

namespace Blunder {

class VulkanAllocator;

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;

  // 绑定描述
  static VkVertexInputBindingDescription getBindingDescription();
  // 属性描述
  static eastl::array<VkVertexInputAttributeDescription, 3>
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
  VmaAllocation getAllocation() const { return m_allocation; }
  VkDeviceSize getSize() const { return m_size; }

 private:
  VulkanAllocator* m_allocator{nullptr};
  VkBuffer m_buffer{VK_NULL_HANDLE};
  VmaAllocation m_allocation{VK_NULL_HANDLE};
  VkDeviceSize m_size{0};
};

}  // namespace Blunder
