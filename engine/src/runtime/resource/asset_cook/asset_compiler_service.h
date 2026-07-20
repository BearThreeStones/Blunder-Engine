#pragma once

#include <cstdint>

#include "EASTL/string.h"

#include "runtime/resource/asset_dependency/asset_dependency_graph.h"

namespace Blunder {

class AssetImportService;
class AssetManager;
class AssetRegistry;
class FileSystem;

struct AssetCompilerStats {
  uint32_t meshes_cooked{0};
  uint32_t textures_cooked{0};
  uint32_t skipped{0};
  uint32_t failed{0};
};

class AssetCompilerService final {
 public:
  void initialize(FileSystem* file_system, AssetManager* asset_manager,
                  AssetRegistry* asset_registry);
  void shutdown();

  /// Optional: run Intermediate Upgrade after registry scan in cookAll.
  void setAssetImportService(AssetImportService* asset_import);

  AssetCompilerStats cookAll(bool force = false);

  /// Optional startup / packaging warm-up: cooks every stale Asset under Assets/.
  /// Pull freshness is defined by markFinalStale / cookAsset / cookDependents,
  /// not by this scan.
  AssetCompilerStats cookIfStale();

  /// Rebuild the held Asset Dependency Graph from the registry + on-disk docs.
  void rebuildDependencyGraph();

  /// How many times rebuildDependencyGraph() has succeeded since initialize.
  uint32_t dependencyGraphRebuildCount() const {
    return m_dependency_graph_rebuild_count;
  }

  /// Delete Final artifacts (mesh/texture bin + meta) for `guid` so the next
  /// cookAsset / load Fast Path treats the Asset as stale.
  void markFinalStale(const eastl::string& guid);

  /// markFinalStale on `guid` and every reverse-edge dependent (transitive).
  /// Call rebuildDependencyGraph() first so the held graph is current.
  void invalidateAssetAndDependents(const eastl::string& guid);

  /// Resolve `guid` via AssetRegistry and cook that one Mesh/Texture descriptor.
  /// Returns true when a new Final was written; false when skipped (fresh),
  /// unknown, or cook failed.
  bool cookAsset(const eastl::string& guid, bool force = false);

  /// cookAsset on reverse-edge dependents of `guid` (direct dependents only).
  /// Call rebuildDependencyGraph() first so the held graph is current.
  void cookDependents(const eastl::string& guid);

 private:
  bool cookMeshDescriptor(const eastl::string& descriptor_virtual_path,
                          bool force);
  bool cookTextureDescriptor(const eastl::string& descriptor_virtual_path,
                             bool force);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  AssetRegistry* m_asset_registry{nullptr};
  AssetImportService* m_asset_import{nullptr};
  AssetDependencyGraph m_dependency_graph;
  uint32_t m_dependency_graph_rebuild_count{0};
  bool m_is_initialized{false};
};

}  // namespace Blunder
