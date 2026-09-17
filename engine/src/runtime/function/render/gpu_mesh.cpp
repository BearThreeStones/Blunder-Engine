#include "runtime/function/render/gpu_mesh.h"

#include <cstddef>

#include "runtime/core/base/macro.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/mesh_skin_data.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"

namespace Blunder {

static_assert(sizeof(MeshVertex) == sizeof(Vertex),
              "MeshVertex must match render Vertex layout");
static_assert(sizeof(SkinnedMeshVertex) == sizeof(SkinnedVertex),
              "SkinnedMeshVertex must match render SkinnedVertex layout");
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
  gpu_mesh->m_vertex_buffer->create(
      allocator, vertex_byte_size,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU);
  gpu_mesh->m_vertex_buffer->upload(vertex_bytes, vertex_byte_size);

  const VkDeviceSize index_byte_size =
      static_cast<VkDeviceSize>(index_count * sizeof(uint32_t));
  gpu_mesh->m_index_buffer = eastl::make_unique<VulkanBuffer>();
  gpu_mesh->m_index_buffer->create(
      allocator, index_byte_size,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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
  eastl::unique_ptr<GpuMesh> gpu_mesh = createInternal(
      allocator, mesh_asset.getVertexData(),
      static_cast<VkDeviceSize>(mesh_asset.getVertexByteSize()),
      mesh_asset.getIndices().data(), mesh_asset.getIndexCount());
  if (gpu_mesh) {
    gpu_mesh->uploadMeshlets(allocator, mesh_asset);
  }
  return gpu_mesh;
}

eastl::unique_ptr<GpuMesh> GpuMesh::createFromGeometry(
    VulkanAllocator* allocator, const void* vertex_bytes,
    VkDeviceSize vertex_byte_size, const uint32_t* indices,
    size_t index_count) {
  return createInternal(allocator, vertex_bytes, vertex_byte_size, indices,
                        index_count);
}

bool GpuMesh::uploadVertices(const void* vertex_bytes, size_t vertex_byte_size) {
  if (m_vertex_buffer == nullptr || vertex_bytes == nullptr ||
      vertex_byte_size == 0) {
    return false;
  }
  if (static_cast<VkDeviceSize>(vertex_byte_size) != m_vertex_buffer->getSize()) {
    return false;
  }
  m_vertex_buffer->upload(vertex_bytes, static_cast<VkDeviceSize>(vertex_byte_size));
  return true;
}

void GpuMesh::destroy() {
  if (m_meshlet_triangle_buffer) {
    m_meshlet_triangle_buffer->destroy();
    m_meshlet_triangle_buffer.reset();
  }
  if (m_meshlet_vertex_buffer) {
    m_meshlet_vertex_buffer->destroy();
    m_meshlet_vertex_buffer.reset();
  }
  if (m_meshlet_index_buffer) {
    m_meshlet_index_buffer->destroy();
    m_meshlet_index_buffer.reset();
  }
  m_meshlet_records.clear();
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

void GpuMesh::uploadMeshlets(VulkanAllocator* allocator,
                             const MeshAsset& mesh_asset) {
  m_meshlet_records.clear();
  if (m_meshlet_index_buffer) {
    m_meshlet_index_buffer->destroy();
    m_meshlet_index_buffer.reset();
  }
  if (m_meshlet_vertex_buffer) {
    m_meshlet_vertex_buffer->destroy();
    m_meshlet_vertex_buffer.reset();
  }
  if (m_meshlet_triangle_buffer) {
    m_meshlet_triangle_buffer->destroy();
    m_meshlet_triangle_buffer.reset();
  }
  if (allocator == nullptr) {
    return;
  }

  const MeshletPayload* payload = nullptr;
  if (mesh_asset.hasMeshlets()) {
    payload = &mesh_asset.getMeshlets();
  }
  if (payload == nullptr) {
    return;
  }
  eastl::vector<uint32_t> expanded;
  expanded.reserve(payload->triangles.size());
  m_meshlet_records.reserve(payload->meshlets.size());
  for (const MeshletRecord& record : payload->meshlets) {
    const uint32_t first_index = static_cast<uint32_t>(expanded.size());
    const uint32_t index_count =
        static_cast<uint32_t>(record.triangle_count) * 3u;
    for (uint32_t t = 0; t < record.triangle_count; ++t) {
      const uint32_t tri = record.triangle_offset + t * 3u;
      for (uint32_t k = 0; k < 3u; ++k) {
        const uint8_t local = payload->triangles[tri + k];
        expanded.push_back(payload->vertices[record.vertex_offset + local]);
      }
    }
    m_meshlet_records.push_back(
        packMeshletGpuRecord(record, first_index, index_count));
  }
  if (expanded.empty()) {
    m_meshlet_records.clear();
    return;
  }

  m_meshlet_index_buffer = eastl::make_unique<VulkanBuffer>();
  const VkDeviceSize bytes =
      static_cast<VkDeviceSize>(expanded.size() * sizeof(uint32_t));
  m_meshlet_index_buffer->create(
      allocator, bytes,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_meshlet_index_buffer->upload(expanded.data(), bytes);

  if (!payload->vertices.empty()) {
    m_meshlet_vertex_buffer = eastl::make_unique<VulkanBuffer>();
    const VkDeviceSize vertex_bytes =
        static_cast<VkDeviceSize>(payload->vertices.size() * sizeof(uint32_t));
    m_meshlet_vertex_buffer->create(
        allocator, vertex_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_meshlet_vertex_buffer->upload(payload->vertices.data(), vertex_bytes);
  }
  if (!payload->triangles.empty()) {
    eastl::vector<uint8_t> padded = payload->triangles;
    while (padded.size() % 4u != 0u) {
      padded.push_back(0);
    }
    m_meshlet_triangle_buffer = eastl::make_unique<VulkanBuffer>();
    const VkDeviceSize tri_bytes =
        static_cast<VkDeviceSize>(padded.size() * sizeof(uint8_t));
    m_meshlet_triangle_buffer->create(
        allocator, tri_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    m_meshlet_triangle_buffer->upload(padded.data(), tri_bytes);
  }
}

}  // namespace Blunder
