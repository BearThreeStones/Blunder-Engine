#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "EASTL/array.h"

namespace Blunder {

class VulkanAllocator;

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};

  static VkVertexInputBindingDescription getBindingDescription();
  static eastl::array<VkVertexInputAttributeDescription, 4>
  getAttributeDescriptions();
};

struct SkinnedVertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
  glm::ivec4 joint_indices{0, 0, 0, 0};
  glm::vec4 weights{1.0f, 0.0f, 0.0f, 0.0f};

  static VkVertexInputBindingDescription getBindingDescription();
  static eastl::array<VkVertexInputAttributeDescription, 6>
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
