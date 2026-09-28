#pragma once

#include <cstdint>

#include "EASTL/functional.h"
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
  /// True when cookAll stopped early (window closed / heartbeat).
  bool aborted{false};
  /// True when cookIfStale reused `.blunder/cooked/cook_stamp` and skipped the
  /// per-descriptor freshness walk.
  bool used_stamp{false};
};

class AssetCompilerService final {
 public:
  void initialize(FileSystem* file_system, AssetManager* asset_manager,
                  AssetRegistry* asset_registry);
  void shutdown();

  /// Optional: run Intermediate Upgrade after registry scan in cookAll.
  void setAssetImportService(AssetImportService* asset_import);

  /// Optional boot pump. Return false to stop cookAll early (window closed).
  void setCookHeartbeat(eastl::function<bool()> heartbeat) {
    m_cook_heartbeat = eastl::move(heartbeat);
  }

  AssetCompilerStats cookAll(bool force = false);

  /// Optional startup / packaging warm-up: cooks every stale Asset under Assets/.
  /// Pull freshness is defined by markFinalStale / cookAsset / cookDependents,
  /// not by this scan.
  /// When `.blunder/cooked/cook_stamp` matches the current Assets+Resources
  /// fingerprint, skips the per-descriptor cook walk (registry scan still runs).
  AssetCompilerStats cookIfStale();

  /// Player boot: if cook stamp fingerprint matches (Assets descriptors +
  /// Resources sources), trust it and skip the cook walk + rebuildFromScan
  /// (registry already loaded from `.blunder/asset_registry.yaml`). Otherwise
  /// falls back to cookIfStale().
  AssetCompilerStats cookIfStaleForPlayer();

  /// Drop the cook stamp so the next cookIfStale rescans descriptors.
  void invalidateCookStamp();

  /// Recompute Assets+Resources fingerprint and write `.blunder/cooked/cook_stamp`.
  /// Call after a completed cookAll(force) so subsequent cookIfStale can skip.
  bool refreshCookStamp();

  /// Rebuild the held Asset Dependency Graph from the registry + on-disk docs.
  void rebuildDependencyGraph();

  /// Direct dependents of `guid` from the held graph (call rebuildDependencyGraph first).
  eastl::vector<eastl::string> dependentsOf(const eastl::string& guid) const;

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
  /// Outcome of cooking one Mesh/Texture descriptor (warm-up accounting).
  enum class DescriptorCookResult {
    Cooked,
    SkippedFresh,
    Failed,
  };

  DescriptorCookResult cookMeshDescriptor(
      const eastl::string& descriptor_virtual_path, bool force);
  DescriptorCookResult cookTextureDescriptor(
      const eastl::string& descriptor_virtual_path, bool force);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  AssetRegistry* m_asset_registry{nullptr};
  AssetImportService* m_asset_import{nullptr};
  eastl::function<bool()> m_cook_heartbeat;
  AssetDependencyGraph m_dependency_graph;
  uint32_t m_dependency_graph_rebuild_count{0};
  bool m_is_initialized{false};
};

}  // namespace Blunder
