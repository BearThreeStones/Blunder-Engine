#pragma once

#include <cstdint>

#include "EASTL/string.h"

namespace Blunder {

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

  AssetCompilerStats cookAll(bool force = false);

  /// Optional startup / packaging warm-up: cooks every stale Asset under Assets/.
  /// Pull freshness is defined by markFinalStale / cookAsset / cookDependents,
  /// not by this scan.
  AssetCompilerStats cookIfStale();

  /// Delete Final artifacts (mesh/texture bin + meta) for `guid` so the next
  /// cookAsset / load Fast Path treats the Asset as stale.
  void markFinalStale(const eastl::string& guid);

  /// Resolve `guid` via AssetRegistry and cook that one Mesh/Texture descriptor.
  /// Returns true when a new Final was written; false when skipped (fresh),
  /// unknown, or cook failed.
  bool cookAsset(const eastl::string& guid, bool force = false);

  /// Stub until the Asset Dependency Graph (OpenSpec 4.x) lands. Currently a
  /// no-op; callers may invoke it after cookAsset / markFinalStale without
  /// fan-out. Do not treat this as cooking dependents yet.
  void cookDependents(const eastl::string& guid);

 private:
  bool cookMeshDescriptor(const eastl::string& descriptor_virtual_path,
                          bool force);
  bool cookTextureDescriptor(const eastl::string& descriptor_virtual_path,
                             bool force);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  AssetRegistry* m_asset_registry{nullptr};
  bool m_is_initialized{false};
};

}  // namespace Blunder
