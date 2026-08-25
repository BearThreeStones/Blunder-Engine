#include "runtime/resource/asset/mesh_material_override.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "EASTL/algorithm.h"

#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

namespace {

void appendUniqueGuid(eastl::vector<eastl::string>& guids,
                      const eastl::string& guid) {
  if (guid.empty()) {
    return;
  }
  if (eastl::find(guids.begin(), guids.end(), guid) != guids.end()) {
    return;
  }
  guids.push_back(guid);
}

void appendSlotGuid(eastl::vector<eastl::string>& guids,
                    const OptionalOverrideSlot& slot) {
  if (slot.present) {
    appendUniqueGuid(guids, slot.guid);
  }
}

MeshOverrideTextureResolver makeTextureResolver(AssetManager* assets,
                                                const AssetRegistry* registry) {
  return [assets, registry](const eastl::string& guid)
             -> eastl::shared_ptr<Texture2DAsset> {
    if (assets == nullptr || registry == nullptr || guid.empty()) {
      return nullptr;
    }
    return assets->loadTexture2DByGuid(guid, *registry);
  };
}

void applySlot(eastl::shared_ptr<Texture2DAsset>& slot_asset,
               AssetHandle* handle, const OptionalOverrideSlot& slot,
               const MeshOverrideTextureResolver& resolve_texture) {
  if (!slot.present) {
    return;
  }
  if (slot.guid.empty()) {
    slot_asset.reset();
    if (handle != nullptr) {
      *handle = {};
    }
    return;
  }
  slot_asset = resolve_texture ? resolve_texture(slot.guid) : nullptr;
  if (handle != nullptr && !slot_asset) {
    *handle = {};
  }
}

}  // namespace

void rebuildMeshTextureGuids(MeshAssetDescriptor& descriptor) {
  if (descriptor.import_texture_guids.empty()) {
    descriptor.import_texture_guids = descriptor.texture_guids;
  }

  eastl::vector<eastl::string> merged = descriptor.import_texture_guids;
  const MeshMaterialOverride& overlay = descriptor.material_override;
  appendSlotGuid(merged, overlay.base_color_texture);
  appendSlotGuid(merged, overlay.metallic_roughness_texture);
  appendSlotGuid(merged, overlay.normal_texture);
  appendSlotGuid(merged, overlay.occlusion_texture);
  descriptor.texture_guids = eastl::move(merged);
}

eastl::shared_ptr<MaterialAsset> cloneMaterialAsset(const MaterialAsset& source) {
  Asset::Meta meta;
  meta.virtual_path = source.getVirtualPath();
  meta.absolute_path = source.getAbsolutePath();
  meta.source_timestamp = source.getSourceTimestamp();
  return eastl::make_shared<MaterialAsset>(
      eastl::move(meta), source.getBaseColorFactor(),
      source.getBaseColorTexture(), source.getBaseColorTextureAsset(),
      source.getMetallicRoughnessTextureAsset(), source.getNormalTextureAsset(),
      source.getOcclusionTextureAsset(), source.getAmbientColor(),
      source.getDiffuseColor(), source.getSpecularColor(), source.getShininess(),
      source.getMetallicFactor(), source.getRoughnessFactor(),
      source.getAlphaMode(), source.getAlphaCutoff(), source.isDoubleSided(),
      source.isUnlit());
}

eastl::shared_ptr<MaterialAsset> makeMeshShadingDefaultMaterial(
    const MeshAsset& mesh) {
  Asset::Meta meta;
  meta.virtual_path = mesh.getVirtualPath() + "#material";
  meta.absolute_path = mesh.getAbsolutePath();
  meta.source_timestamp = mesh.getSourceTimestamp();
  return eastl::make_shared<MaterialAsset>(
      eastl::move(meta), glm::vec4(1.0f), AssetHandle{}, nullptr, nullptr,
      nullptr, nullptr, glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.4f),
      32.0f, 1.0f, 1.0f, cgltf_alpha_mode_opaque, 0.5f, false, false);
}

eastl::shared_ptr<MeshAsset> cloneMeshAsset(
    const MeshAsset& source, eastl::shared_ptr<MaterialAsset> material) {
  Asset::Meta meta;
  meta.virtual_path = source.getVirtualPath();
  meta.absolute_path = source.getAbsolutePath();
  meta.source_timestamp = source.getSourceTimestamp();
  return eastl::make_shared<MeshAsset>(
      eastl::move(meta), eastl::vector<MeshVertex>(source.getVertices()),
      eastl::vector<uint32_t>(source.getIndices()), source.getMaterial(),
      eastl::move(material), source.getSkinData(), source.isFromCookedFinal());
}

void applyMeshMaterialOverride(
    MaterialAsset& material, const MeshMaterialOverride& overlay,
    const MeshOverrideTextureResolver& resolve_texture) {
  if (overlay.unlit.present) {
    material.setUnlit(overlay.unlit.value);
  }
  if (overlay.base_color.present) {
    material.setBaseColorFactor(overlay.base_color.value);
  }
  if (overlay.metallic.present) {
    material.setMetallicFactor(overlay.metallic.value);
  }
  if (overlay.roughness.present) {
    material.setRoughnessFactor(overlay.roughness.value);
  }
  if (overlay.ambient.present) {
    material.setAmbientColor(overlay.ambient.value);
  }
  if (overlay.diffuse.present) {
    material.setDiffuseColor(overlay.diffuse.value);
  }
  if (overlay.specular.present) {
    material.setSpecularColor(overlay.specular.value);
  }
  if (overlay.shininess.present) {
    material.setShininess(overlay.shininess.value);
  }

  AssetHandle base_color_handle = material.getBaseColorTexture();
  eastl::shared_ptr<Texture2DAsset> base_color =
      material.getBaseColorTextureAsset();
  eastl::shared_ptr<Texture2DAsset> metallic_roughness =
      material.getMetallicRoughnessTextureAsset();
  eastl::shared_ptr<Texture2DAsset> normal = material.getNormalTextureAsset();
  eastl::shared_ptr<Texture2DAsset> occlusion =
      material.getOcclusionTextureAsset();

  applySlot(base_color, &base_color_handle, overlay.base_color_texture,
            resolve_texture);
  applySlot(metallic_roughness, nullptr, overlay.metallic_roughness_texture,
            resolve_texture);
  applySlot(normal, nullptr, overlay.normal_texture, resolve_texture);
  applySlot(occlusion, nullptr, overlay.occlusion_texture, resolve_texture);

  material.setBaseColorTexture(base_color_handle, eastl::move(base_color));
  material.setMetallicRoughnessTextureAsset(eastl::move(metallic_roughness));
  material.setNormalTextureAsset(eastl::move(normal));
  material.setOcclusionTextureAsset(eastl::move(occlusion));
}

void reapplyMeshMaterialOverride(
    MeshAsset& yaml_mesh, const MeshAsset* import_source,
    const MeshMaterialOverride& overlay, AssetManager* assets,
    const AssetRegistry* registry) {
  eastl::shared_ptr<MaterialAsset> working;
  if (import_source != nullptr && import_source->getMaterialAsset()) {
    working = cloneMaterialAsset(*import_source->getMaterialAsset());
  } else if (yaml_mesh.getMaterialAsset() && overlay.empty()) {
    return;
  } else {
    working = makeMeshShadingDefaultMaterial(yaml_mesh);
  }
  applyMeshMaterialOverride(*working, overlay,
                            makeTextureResolver(assets, registry));
  yaml_mesh.setMaterialAsset(eastl::move(working));
}

eastl::shared_ptr<MeshAsset> instantiateMeshWithMaterialOverride(
    const eastl::shared_ptr<MeshAsset>& source,
    const MeshAssetDescriptor& descriptor, AssetManager* assets,
    const AssetRegistry* registry) {
  if (!source) {
    return nullptr;
  }
  eastl::shared_ptr<MeshAsset> yaml_mesh =
      cloneMeshAsset(*source, source->getMaterialAsset());
  if (!descriptor.material_override.empty()) {
    reapplyMeshMaterialOverride(*yaml_mesh, source.get(),
                                descriptor.material_override, assets, registry);
  }
  return yaml_mesh;
}

}  // namespace Blunder
