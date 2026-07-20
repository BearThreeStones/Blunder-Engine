#pragma once

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

class FileSystem;

/// Maps GUID → assets/.../descriptor.yaml under `.blunder/asset_registry.yaml`.
class AssetRegistry final {
 public:
  void initialize(FileSystem* file_system);
  void shutdown();

  eastl::string allocateGuid();
  bool registerAsset(const eastl::string& guid,
                       const eastl::string& descriptor_virtual_path);
  bool unregisterGuid(const eastl::string& guid);

  eastl::string resolveGuid(const eastl::string& guid) const;
  eastl::string findGuidForPath(
      const eastl::string& descriptor_virtual_path) const;

  /// Rescans Assets/ for *.mesh.yaml, *.texture.yaml, and *.scene.asset and
  /// rebuilds the map (only files with a valid guid field).
  void rebuildFromScan();

  /// Ensures a `.scene.asset` has a guid on disk and is registered.
  /// Allocates + persists guid when missing; registers when already present.
  bool ensureSceneAssetRegistered(const eastl::string& scene_virtual_path);

  /// Snapshot of registered GUID → descriptor virtual path entries.
  eastl::vector<eastl::pair<eastl::string, eastl::string>>
  registeredEntries() const;

  bool save() const;
  bool load();

 private:
  eastl::string registryPath() const;

  FileSystem* m_file_system{nullptr};
  eastl::hash_map<eastl::string, eastl::string> m_guid_to_path;
  eastl::hash_map<eastl::string, eastl::string> m_path_to_guid;
  bool m_is_initialized{false};
};

}  // namespace Blunder
