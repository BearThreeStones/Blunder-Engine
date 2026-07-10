#include "runtime/function/render/pick/pick_instance_buffer.h"

#include <cgltf.h>
#include <glm/vec4.hpp>
#include <vk_mem_alloc.h>

#include "runtime/function/render/overlay/pick_overlay.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/render/vulkan/vulkan_allocator.h"
#include "runtime/function/render/vulkan/vulkan_buffer.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {
namespace {

VulkanTexture* resolveBaseColorTexture(RenderSystem& render_system,
                                       const MeshRendererComponent& renderer) {
  if (!renderer.material) {
    return render_system.getFallbackTexture();
  }
  const eastl::shared_ptr<Texture2DAsset>& texture_asset =
      renderer.material->getBaseColorTextureAsset();
  if (!texture_asset) {
    return render_system.getFallbackTexture();
  }
  VulkanTexture* uploaded = render_system.ensureTextureUploaded(texture_asset.get());
  return uploaded != nullptr ? uploaded : render_system.getFallbackTexture();
}

AABB computeWorldBounds(const MeshAsset& mesh, const glm::mat4& world_matrix) {
  AABB bounds{};
  bool has_bounds = false;
  for (const MeshVertex& vertex : mesh.getVertices()) {
    const glm::vec3 world =
        glm::vec3(world_matrix * glm::vec4(vertex.position, 1.0f));
    if (!has_bounds) {
      bounds.min = bounds.max = world;
      has_bounds = true;
    } else {
      bounds.expandToInclude(world);
    }
  }
  return bounds;
}

void writeWorldAabb(PickInstanceGpu& row, const AABB& bounds) {
  row.aabb_min[0] = bounds.min.x;
  row.aabb_min[1] = bounds.min.y;
  row.aabb_min[2] = bounds.min.z;
  row.aabb_max[0] = bounds.max.x;
  row.aabb_max[1] = bounds.max.y;
  row.aabb_max[2] = bounds.max.z;
}

}  // namespace

PickInstanceBuffer::~PickInstanceBuffer() { shutdownGpu(); }

void PickInstanceBuffer::rebuild(SceneInstance& scene, RenderSystem* render_system) {
  m_draws.clear();
  m_instances.clear();

  scene.forEachMeshRenderer([&](EntityId entity_id,
                                const MeshRendererComponent& renderer) {
    if (!renderer.mesh || renderer.alpha_mode == cgltf_alpha_mode_blend) {
      return;
    }

    const glm::mat4 world_matrix = scene.getWorldMatrix(entity_id);
    const AABB world_bounds = computeWorldBounds(*renderer.mesh, world_matrix);

    PickOverlay::PickDraw draw{};
    bool has_draw = false;
    if (render_system != nullptr) {
      GpuMesh* gpu_mesh = render_system->getOrUploadGpuMesh(renderer.mesh.get());
      if (gpu_mesh == nullptr || gpu_mesh->getVertexBuffer() == nullptr ||
          gpu_mesh->getIndexBuffer() == nullptr || gpu_mesh->getIndexCount() == 0) {
        return;
      }

      draw.gpu_mesh = gpu_mesh;
      draw.world_matrix = world_matrix;
      draw.entity_id = entity_id;
      draw.alpha_mode = renderer.alpha_mode;
      draw.alpha_cutoff = renderer.alpha_cutoff;
      if (renderer.material) {
        draw.base_color_alpha = renderer.material->getBaseColorFactor().a;
        draw.has_base_color_texture = renderer.material->hasBaseColorTexture();
        draw.base_color_texture = resolveBaseColorTexture(*render_system, renderer);
      } else {
        draw.base_color_texture = render_system->getFallbackTexture();
      }
      has_draw = true;
    }

    const Entity* entity = scene.getEntity(entity_id);
    const EntityId parent_id =
        entity != nullptr ? entity->getParentId() : k_invalid_entity_id;

    PickInstanceGpu row{};
    row.entity_id = static_cast<uint32_t>(entity_id);
    row.parent_id = static_cast<uint32_t>(parent_id);
    row.flags = k_pick_instance_pickable;
    row.draw_index = static_cast<uint32_t>(m_draws.size());
    writeWorldAabb(row, world_bounds);
    m_instances.push_back(row);

    if (has_draw) {
      m_draws.push_back(draw);
    }
  });

  ++m_generation;
}

void PickInstanceBuffer::uploadToGpu(VulkanAllocator* alloc) {
  if (alloc == nullptr) {
    return;
  }

  const VkDeviceSize byte_size =
      static_cast<VkDeviceSize>(m_instances.size() * sizeof(PickInstanceGpu));
  if (byte_size == 0) {
    shutdownGpu();
    return;
  }

  if (!m_gpu_buffer || m_gpu_buffer->getSize() < byte_size) {
    shutdownGpu();
    m_gpu_buffer = eastl::make_unique<VulkanBuffer>();
    m_gpu_buffer->create(alloc, byte_size,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         VMA_MEMORY_USAGE_CPU_TO_GPU);
  }

  m_gpu_buffer->upload(m_instances.data(), byte_size);
}

void PickInstanceBuffer::shutdownGpu() {
  if (m_gpu_buffer) {
    m_gpu_buffer->destroy();
    m_gpu_buffer.reset();
  }
}

VkBuffer PickInstanceBuffer::gpuBuffer() const {
  return m_gpu_buffer ? m_gpu_buffer->getBuffer() : VK_NULL_HANDLE;
}

}  // namespace Blunder
