#pragma once

#include <cstddef>
#include <filesystem>

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"

struct cgltf_data;

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

}  // namespace Blunder
