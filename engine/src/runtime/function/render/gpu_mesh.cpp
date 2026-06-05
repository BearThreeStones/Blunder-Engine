#include "runtime/function/render/gpu_mesh.h"

#include <cstddef>

#include "runtime/core/base/macro.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"

namespace Blunder {

static_assert(sizeof(MeshVertex) == sizeof(Vertex),
              "MeshVertex must match render Vertex layout");
static_assert(offsetof(MeshVertex, position) == offsetof(Vertex, position),
              "MeshVertex/Vertex position offset mismatch");
static_assert(offsetof(MeshVertex, normal) == offsetof(Vertex, normal),
              "MeshVertex/Vertex normal offset mismatch");
static_assert(offsetof(MeshVertex, uv) == offsetof(Vertex, uv),
              "MeshVertex/Vertex uv offset mismatch");
static_assert(offsetof(MeshVertex, tangent) == offsetof(Vertex, tangent),
              "MeshVertex/Vertex tangent offset mismatch");

eastl::unique_ptr<GpuMesh> GpuMesh::createInternal(
    VulkanAllocator* allocator, const void* vertex_bytes,
    VkDeviceSize vertex_byte_size, const uint32_t* indices, size_t index_count) {
  if (allocator == nullptr || vertex_bytes == nullptr || indices == nullptr ||
      vertex_byte_size == 0 || index_count == 0) {
    return nullptr;
  }

  eastl::unique_ptr<GpuMesh> gpu_mesh(new GpuMesh());

  gpu_mesh->m_vertex_buffer = eastl::make_unique<VulkanBuffer>();
  gpu_mesh->m_vertex_buffer->create(allocator, vertex_byte_size,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VMA_MEMORY_USAGE_CPU_TO_GPU);
  gpu_mesh->m_vertex_buffer->upload(vertex_bytes, vertex_byte_size);

  const VkDeviceSize index_byte_size =
      static_cast<VkDeviceSize>(index_count * sizeof(uint32_t));
  gpu_mesh->m_index_buffer = eastl::make_unique<VulkanBuffer>();
  gpu_mesh->m_index_buffer->create(allocator, index_byte_size,
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);
  gpu_mesh->m_index_buffer->upload(indices, index_byte_size);

  gpu_mesh->m_index_count = static_cast<uint32_t>(index_count);
  return gpu_mesh;
}

eastl::unique_ptr<GpuMesh> GpuMesh::create(VulkanAllocator* allocator,
                                           const MeshAsset& mesh_asset) {
  if (mesh_asset.getVertexCount() == 0 || mesh_asset.getIndexCount() == 0) {
    return nullptr;
  }
  return createInternal(allocator, mesh_asset.getVertexData(),
                        static_cast<VkDeviceSize>(mesh_asset.getVertexByteSize()),
                        mesh_asset.getIndices().data(),
                        mesh_asset.getIndexCount());
}

eastl::unique_ptr<GpuMesh> GpuMesh::createFromGeometry(
    VulkanAllocator* allocator, const void* vertex_bytes,
    VkDeviceSize vertex_byte_size, const uint32_t* indices,
    size_t index_count) {
  return createInternal(allocator, vertex_bytes, vertex_byte_size, indices,
                        index_count);
}

void GpuMesh::destroy() {
  if (m_index_buffer) {
    m_index_buffer->destroy();
    m_index_buffer.reset();
  }
  if (m_vertex_buffer) {
    m_vertex_buffer->destroy();
    m_vertex_buffer.reset();
  }
  m_index_count = 0;
}

}  // namespace Blunder
