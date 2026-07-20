#pragma once

#include <cstdint>
#include <filesystem>

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/unordered_map.h"
#include "EASTL/vector.h"

#include <cgltf.h>

#include "runtime/resource/asset/asset.h"
#include "runtime/resource/asset/material_asset.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/scene_asset.h"
#include "runtime/resource/asset/shader_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_manager/asset_manager_gltf.h"

namespace Blunder {

class AssetRegistry;
class FileSystem;

struct AssetManagerInitInfo {
  /// Required. AssetManager performs all IO through this.
  FileSystem* file_system{nullptr};
};

template <typename T>
struct AssetTypeTraits;

template <>
struct AssetTypeTraits<Texture2DAsset> {
  static constexpr Asset::Type k_type = Asset::Type::Texture2D;
};

template <>
struct AssetTypeTraits<ShaderAsset> {
  static constexpr Asset::Type k_type = Asset::Type::Shader;
};

template <>
struct AssetTypeTraits<MeshAsset> {
  static constexpr Asset::Type k_type = Asset::Type::Mesh;
};

template <>
struct AssetTypeTraits<MaterialAsset> {
  static constexpr Asset::Type k_type = Asset::Type::Material;
};

template <>
struct AssetTypeTraits<SceneAsset> {
  static constexpr Asset::Type k_type = Asset::Type::Scene;
};

/// AssetManager owns the lifetime of CPU-side resources loaded from disk.
///
/// Lookup is by virtual path (relative to the FileSystem's asset root). A
/// per-type weak cache lets identical paths share a single in-memory copy
/// while still letting assets be released once nothing holds a strong handle.
class AssetManager final {
 public:
  AssetManager() = default;
  ~AssetManager() = default;

  AssetManager(const AssetManager&) = delete;
  AssetManager& operator=(const AssetManager&) = delete;

  void initialize(const AssetManagerInitInfo& info);
  void shutdown();

  // ---- Loading -----------------------------------------------------------

  /// Load (or fetch from cache) a 2D texture. `virtual_path` is interpreted
  /// relative to the FileSystem's asset root; absolute paths are also
  /// accepted. Returns nullptr on failure (the call site is responsible for
  /// reacting; the manager logs the cause).
  ///
  /// The returned texture is RGBA8 (4 channels), regardless of source
  /// channel count, to keep upload paths uniform.
  eastl::shared_ptr<Texture2DAsset> loadTexture2D(
      const eastl::string& virtual_path);
  eastl::shared_ptr<ShaderAsset> loadShader(const eastl::string& virtual_path);
  eastl::shared_ptr<MeshAsset> loadMesh(const eastl::string& virtual_path);
  eastl::shared_ptr<MeshAsset> loadMeshPrimitive(
      cgltf_data* data, size_t mesh_index, size_t primitive_index,
      const std::filesystem::path& absolute,
      const eastl::string& gltf_canonical_key);
  eastl::shared_ptr<MaterialAsset> loadGltfMaterial(
      cgltf_data* data, size_t material_index,
      const std::filesystem::path& absolute,
      const eastl::string& gltf_canonical_key);
  eastl::shared_ptr<SceneAsset> loadScene(const eastl::string& virtual_path);

  /// Resolve GUID → descriptor virtual path via registry. Empty if unknown.
  /// Path-based APIs remain for migration and display.
  eastl::string resolveGuidPath(const eastl::string& guid,
                                const AssetRegistry& registry) const;

  /// Load by GUID: resolve descriptor path then call the path-based load*.
  eastl::shared_ptr<MeshAsset> loadMeshByGuid(const eastl::string& guid,
                                              const AssetRegistry& registry);
  eastl::shared_ptr<Texture2DAsset> loadTexture2DByGuid(
      const eastl::string& guid, const AssetRegistry& registry);
  eastl::shared_ptr<SceneAsset> loadSceneByGuid(const eastl::string& guid,
                                                const AssetRegistry& registry);

  /// Opens a glTF/GLB file for import. On success, `out_document.data` must be
  /// released with closeGltfImportDocument().
  bool openGltfImportDocument(const eastl::string& virtual_path,
                              GltfImportDocument& out_document);
  void closeGltfImportDocument(GltfImportDocument& document);

  /// Resolves a `.mesh.yaml` / `.mesh.asset` descriptor or glTF path to a glTF source path.
  bool resolveGltfSourcePath(const eastl::string& virtual_path,
                             eastl::string& out_gltf_source) const;

  AssetHandle makeHandle(Asset::Type type, const eastl::string& virtual_path) const;
  eastl::shared_ptr<Asset> get(const AssetHandle& handle) const;

  template <typename T>
  eastl::shared_ptr<T> getTyped(const AssetHandle& handle) const {
    if (handle.type != AssetTypeTraits<T>::k_type) {
      return nullptr;
    }
    auto base = get(handle);
    if (!base || base->getType() != AssetTypeTraits<T>::k_type) {
      return nullptr;
    }
    return eastl::static_pointer_cast<T>(base);
  }

  // ---- Maintenance -------------------------------------------------------

  /// Drop expired cache entries (entries whose only reference was the cache
  /// itself). Safe to call any time; cheap O(n) over the cache.
  void garbageCollect();

  /// Drop all cached references. Strong handles held elsewhere stay valid
  /// until their owners release them.
  void clearCache();

 private:
  template <typename T>
  using Cache = eastl::unordered_map<eastl::string, eastl::weak_ptr<T>>;

  eastl::string canonicalKey(const eastl::string& virtual_path) const;

  FileSystem* m_file_system{nullptr};
  Cache<Texture2DAsset> m_texture_cache;
  Cache<ShaderAsset> m_shader_cache;
  Cache<MeshAsset> m_mesh_cache;
  Cache<MaterialAsset> m_material_cache;
  Cache<SceneAsset> m_scene_cache;
  bool m_is_initialized{false};
};

}  // namespace Blunder
