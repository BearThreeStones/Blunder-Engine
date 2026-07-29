#include "runtime/function/scene/scene_render_bridge.h"

#include <cgltf.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/overlay/overlay_system.h"
#include "runtime/function/render/render_system.h"
#include "runtime/function/scene/cpu_skinning.h"
#include "runtime/function/scene/gpu_skinning.h"
#include "runtime/function/scene/mesh_renderer_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/function/render/vulkan/vulkan_texture.h"

namespace Blunder {

namespace {

VulkanTexture* resolveTexture(RenderSystem* render_system,
                            const eastl::shared_ptr<Texture2DAsset>& texture_asset,
                            VulkanTexture* fallback) {
  if (render_system == nullptr || texture_asset == nullptr) {
    return fallback;
  }
  VulkanTexture* uploaded = render_system->ensureTextureUploaded(texture_asset.get());
  return uploaded != nullptr ? uploaded : fallback;
}

eastl::string meshGpuCacheKey(const MeshAsset& mesh_asset) {
  eastl::string cache_key = mesh_asset.getVirtualPath();
  if (cache_key.empty()) {
    const std::filesystem::path& absolute_path = mesh_asset.getAbsolutePath();
    if (!absolute_path.empty()) {
      cache_key = eastl::string(absolute_path.generic_string().c_str());
    } else {
      cache_key = "generated://render/anonymous_mesh";
    }
  }
  return cache_key;
}

void submitMeshDraw(RenderSystem* render_system, GpuMesh* gpu_mesh,
                    const MeshRendererComponent& draw_renderer,
                    VulkanTexture* fallback_texture,
                    eastl::vector<glm::mat4> gpu_bone_palette) {
  eastl::shared_ptr<MaterialAsset> material = draw_renderer.material;
  VulkanTexture* base_color_texture = fallback_texture;
  VulkanTexture* metallic_roughness_texture = fallback_texture;
  VulkanTexture* normal_texture = fallback_texture;
  VulkanTexture* occlusion_texture = fallback_texture;

  if (material) {
    base_color_texture = resolveTexture(render_system,
                                          material->getBaseColorTextureAsset(),
                                          fallback_texture);
    metallic_roughness_texture = resolveTexture(
        render_system, material->getMetallicRoughnessTextureAsset(),
        fallback_texture);
    normal_texture =
        resolveTexture(render_system, material->getNormalTextureAsset(),
                       fallback_texture);
    occlusion_texture = resolveTexture(render_system,
                                       material->getOcclusionTextureAsset(),
                                       fallback_texture);
  }

  const bool is_blend = draw_renderer.alpha_mode == cgltf_alpha_mode_blend;
  if (is_blend) {
    render_system->addTransparentMeshDraw(
        gpu_mesh, material, base_color_texture, metallic_roughness_texture,
        normal_texture, occlusion_texture, draw_renderer.world_matrix,
        draw_renderer.alpha_cutoff, draw_renderer.double_sided,
        eastl::move(gpu_bone_palette));
  } else {
    render_system->addOpaqueMeshDraw(
        gpu_mesh, material, base_color_texture, metallic_roughness_texture,
        normal_texture, occlusion_texture, draw_renderer.world_matrix,
        draw_renderer.alpha_cutoff, draw_renderer.alpha_mode,
        draw_renderer.double_sided, eastl::move(gpu_bone_palette));
  }
}

}  // namespace

void syncSceneToRender(RenderSystem* render_system, SceneInstance* scene_instance) {
  if (render_system == nullptr) {
    return;
  }

  if (OverlaySystem* overlay = render_system->getOverlaySystem()) {
    overlay->markPickInstancesDirty();
  }

  render_system->clearOpaqueMeshDraws();
  render_system->clearTransparentMeshDraws();

  if (scene_instance == nullptr) {
    return;
  }

  VulkanTexture* fallback_texture = render_system->getFallbackTexture();
  eastl::vector<MeshVertex> skinned_vertices_scratch;
  eastl::vector<SkinnedMeshVertex> gpu_skinned_vertices_scratch;
  eastl::vector<glm::mat4> bone_palette_scratch;

  scene_instance->forEachMeshRenderer(
      [&](EntityId entity_id, const MeshRendererComponent& renderer) {
        if (!renderer.mesh) {
          return;
        }

        MeshRendererComponent draw_renderer = renderer;
        draw_renderer.world_matrix = scene_instance->getWorldMatrix(entity_id);

        GpuMesh* gpu_mesh = nullptr;
        eastl::vector<glm::mat4> gpu_bone_palette;
        const eastl::string cache_key = meshGpuCacheKey(*renderer.mesh);

        if (renderer.mesh->isSkinned()) {
          Skeleton* skeleton = scene_instance->findSkeletonForEntity(entity_id);
          if (skeleton != nullptr) {
            if (shouldUseGpuSkinning(*renderer.mesh)) {
              packSkinnedMeshVertices(*renderer.mesh, gpu_skinned_vertices_scratch);
              gpu_mesh = render_system->getOrUploadGpuMeshByKey(
                  cache_key + "#gpu_skinned", gpu_skinned_vertices_scratch.data(),
                  gpu_skinned_vertices_scratch.size() * sizeof(SkinnedMeshVertex),
                  renderer.mesh->getIndices().data(),
                  renderer.mesh->getIndexCount());
              buildGpuBonePalette(*skeleton, renderer.mesh->getSkinData(),
                                  bone_palette_scratch);
              gpu_bone_palette = bone_palette_scratch;
            } else {
              applyCpuSkinning(*skeleton, renderer.mesh->getSkinData(),
                               renderer.mesh->getVertices(),
                               skinned_vertices_scratch);
              gpu_mesh = render_system->updateOrUploadSkinnedGpuMesh(
                  cache_key, skinned_vertices_scratch.data(),
                  skinned_vertices_scratch.size() * sizeof(MeshVertex),
                  renderer.mesh->getIndices().data(),
                  renderer.mesh->getIndexCount());
            }
          }
        }

        if (gpu_mesh == nullptr) {
          gpu_mesh = render_system->getOrUploadGpuMesh(renderer.mesh.get());
        }
        if (gpu_mesh == nullptr) {
          return;
        }

        submitMeshDraw(render_system, gpu_mesh, draw_renderer, fallback_texture,
                       eastl::move(gpu_bone_palette));
      });
}

}  // namespace Blunder
