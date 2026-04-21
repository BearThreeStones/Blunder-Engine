#pragma once

#include <vulkan/vulkan.h>

// Forward declare VMA types without including header
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;

namespace Blunder {

class VulkanContext;

class VulkanAllocator final {
 public:
  VulkanAllocator() = default;
  ~VulkanAllocator() = default;

  void initialize(VulkanContext* context);
  void shutdown();

  VmaAllocator getAllocator() const { return m_allocator; }

 private:
  VulkanContext* m_context{nullptr};
  VmaAllocator m_allocator{nullptr};
};

}  // namespace Blunder
