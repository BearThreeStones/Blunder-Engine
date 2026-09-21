#pragma once

#include <cstddef>
#include <filesystem>

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/mesh_asset.h"

struct cgltf_data;
struct cgltf_primitive;

namespace Blunder {

class AssetManager;
class MaterialAsset;
class MeshAsset;

/// Parsed glTF document for scene import (caller must call closeGltfImportDocument).
struct GltfImportDocument {
  std::filesystem::path absolute;
  eastl::string canonical_key;
  cgltf_data* data{nullptr};
};

eastl::string makeMeshPrimitiveCacheKey(const eastl::string& gltf_canonical_key,
                                       size_t mesh_index, size_t primitive_index);

eastl::string makeGltfMaterialCacheKey(const eastl::string& gltf_canonical_key,
                                       size_t material_index);

/// POSITION/NORMAL/TEXCOORD_0/TANGENT/COLOR_0 plus indices. COLOR_0 missing
/// stays white (1,1,1,1). No FATAL.
bool readGltfPrimitiveGeometry(const cgltf_primitive& primitive,
                               eastl::vector<MeshVertex>& out_vertices,
                               eastl::vector<uint32_t>& out_indices);

}  // namespace Blunder
