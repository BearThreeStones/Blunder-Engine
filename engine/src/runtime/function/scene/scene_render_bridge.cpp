#include "runtime/function/scene/scene_render_bridge.h"

#include <cgltf.h>

#include "runtime/core/base/macro.h"
#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/render/render_system.h"
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

}  // namespace

void syncSceneToRender(RenderSystem* render_system, SceneInstance* scene_instance) {
  if (render_system == nullptr) {
    return;
  }

  render_system->clearOpaqueMeshDraws();
  render_system->clearTransparentMeshDraws();

  if (scene_instance == nullptr) {
    return;
  }

  VulkanTexture* fallback_texture = render_system->getFallbackTexture();

  scene_instance->forEachMeshRenderer(
      [&](EntityId entity_id, const MeshRendererComponent& renderer) {
        (void)entity_id;
        if (!renderer.mesh) {
          return;
        }

        GpuMesh* gpu_mesh = render_system->getOrUploadGpuMesh(renderer.mesh.get());
        if (gpu_mesh == nullptr) {
          return;
        }

        eastl::shared_ptr<MaterialAsset> material = renderer.material;
        VulkanTexture* base_color_texture = fallback_texture;
        VulkanTexture* metallic_roughness_texture = fallback_texture;
        VulkanTexture* normal_texture = fallback_texture;
        VulkanTexture* occlusion_texture = fallback_texture;

        if (material) {
          base_color_texture =
              resolveTexture(render_system, material->getBaseColorTextureAsset(),
                             fallback_texture);
          metallic_roughness_texture = resolveTexture(
              render_system, material->getMetallicRoughnessTextureAsset(),
              fallback_texture);
          normal_texture = resolveTexture(render_system,
                                          material->getNormalTextureAsset(),
                                          fallback_texture);
          occlusion_texture = resolveTexture(render_system,
                                             material->getOcclusionTextureAsset(),
                                             fallback_texture);
        }

        const bool is_blend = renderer.alpha_mode == cgltf_alpha_mode_blend;

        if (is_blend) {
          render_system->addTransparentMeshDraw(
              gpu_mesh, material, base_color_texture, metallic_roughness_texture,
              normal_texture, occlusion_texture, renderer.world_matrix,
              renderer.alpha_cutoff, renderer.double_sided);
        } else {
          render_system->addOpaqueMeshDraw(
              gpu_mesh, material, base_color_texture, metallic_roughness_texture,
              normal_texture, occlusion_texture, renderer.world_matrix,
              renderer.alpha_cutoff, renderer.alpha_mode, renderer.double_sided);
        }
      });
}

}  // namespace Blunder
