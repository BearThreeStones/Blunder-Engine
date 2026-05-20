#pragma once

#include <cstddef>
#include <cstdint>

#include <vulkan/vulkan.h>

#include "EASTL/unique_ptr.h"

namespace Blunder {

class MeshAsset;
class VulkanAllocator;
class VulkanBuffer;

/// GPU-resident indexed mesh (shared vertex/index buffers).
class GpuMesh final {
 public:
  static eastl::unique_ptr<GpuMesh> create(VulkanAllocator* allocator,
                                           const MeshAsset& mesh_asset);

  static eastl::unique_ptr<GpuMesh> createFromGeometry(
      VulkanAllocator* allocator, const void* vertex_bytes,
      VkDeviceSize vertex_byte_size, const uint32_t* indices,
      size_t index_count);

  ~GpuMesh() = default;

  void destroy();

  VulkanBuffer* getVertexBuffer() const { return m_vertex_buffer.get(); }
  VulkanBuffer* getIndexBuffer() const { return m_index_buffer.get(); }
  uint32_t getIndexCount() const { return m_index_count; }

 private:
  GpuMesh() = default;

  static eastl::unique_ptr<GpuMesh> createInternal(
      VulkanAllocator* allocator, const void* vertex_bytes,
      VkDeviceSize vertex_byte_size, const uint32_t* indices, size_t index_count);

  eastl::unique_ptr<VulkanBuffer> m_vertex_buffer;
  eastl::unique_ptr<VulkanBuffer> m_index_buffer;
  uint32_t m_index_count{0};
};

}  // namespace Blunder
