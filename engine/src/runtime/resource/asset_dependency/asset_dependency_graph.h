#pragma once

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

class AssetRegistry;
class FileSystem;

/// Freshness leaf inputs for an Asset (descriptor + Intermediate source).
struct AssetDependencyLeaves {
  eastl::string descriptor_virtual_path;
  eastl::string intermediate_source_path;
};

/// Minimal Asset Dependency Graph: Scene→Mesh, Mesh→Texture (explicit),
/// plus Intermediate leaf side-table for freshness keys.
class AssetDependencyGraph final {
 public:
  void clear();

  /// Rebuild reverse edges and leaf table from registry + on-disk documents.
  void rebuildFromProject(FileSystem& file_system,
                          const AssetRegistry& registry);

  /// Assets that depend on `guid` (reverse edges).
  eastl::vector<eastl::string> dependentsOf(const eastl::string& guid) const;

  AssetDependencyLeaves intermediateLeavesOf(
      const eastl::string& guid) const;

 private:
  void addDependent(const eastl::string& dependency_guid,
                    const eastl::string& dependent_guid);

  eastl::hash_map<eastl::string, eastl::vector<eastl::string>> m_dependents;
  eastl::hash_map<eastl::string, AssetDependencyLeaves> m_leaves;
};

}  // namespace Blunder
