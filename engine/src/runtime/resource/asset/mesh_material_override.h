#pragma once

#include "EASTL/functional.h"
#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AssetManager;
class AssetRegistry;
class MaterialAsset;
class MeshAsset;
class Texture2DAsset;

using MeshOverrideTextureResolver =
    eastl::function<eastl::shared_ptr<Texture2DAsset>(const eastl::string&)>;

eastl::shared_ptr<MaterialAsset> cloneMaterialAsset(const MaterialAsset& source);

eastl::shared_ptr<MaterialAsset> makeMeshShadingDefaultMaterial(
    const MeshAsset& mesh);

eastl::shared_ptr<MeshAsset> cloneMeshAsset(
    const MeshAsset& source, eastl::shared_ptr<MaterialAsset> material);

void applyMeshMaterialOverride(
    MaterialAsset& material, const MeshMaterialOverride& overlay,
    const MeshOverrideTextureResolver& resolve_texture);

/// Clone `source` into a descriptor-keyed MeshAsset and overlay the sparse bag
/// onto a private MaterialAsset. Extra primitives stay Import-built.
eastl::shared_ptr<MeshAsset> instantiateMeshWithMaterialOverride(
    const eastl::shared_ptr<MeshAsset>& source,
    const MeshAssetDescriptor& descriptor, AssetManager* assets,
    const AssetRegistry* registry);

void reapplyMeshMaterialOverride(
    MeshAsset& yaml_mesh, const MeshAsset* import_source,
    const MeshMaterialOverride& overlay, AssetManager* assets,
    const AssetRegistry* registry);

}  // namespace Blunder
