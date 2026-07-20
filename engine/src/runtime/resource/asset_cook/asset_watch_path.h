#pragma once

#include <cstdint>
#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

class AssetDependencyGraph;
class AssetRegistry;

/// Classification of a filesystem event path for Asset Watch (task 4.3+).
enum class AssetWatchPathClass : uint8_t {
  Ignored = 0,
  AssetsTree,             // under Assets/ — browser refresh + descriptor invalidate
  IntermediateResource,   // under Resources/ excluding Source — Intermediate invalidate
  SourceArchive,          // under Resources/Source/ — Reimport later (4.4); no Intermediate invalidate
};

/// Classify an absolute file path relative to Assets and Resources roots.
/// SourceArchive is never treated as IntermediateResource.
AssetWatchPathClass classifyAssetWatchPath(
    const std::filesystem::path& absolute_file_path,
    const std::filesystem::path& assets_root,
    const std::filesystem::path& resources_root);

/// Map a classified watch path to Asset GUID(s) that should receive
/// invalidateAssetAndDependents. SourceArchive and Ignored return empty.
eastl::vector<eastl::string> guidsToInvalidateForWatchedPath(
    AssetWatchPathClass path_class,
    const std::filesystem::path& absolute_file_path,
    const std::filesystem::path& assets_root,
    const std::filesystem::path& resources_root,
    const AssetRegistry& registry,
    const AssetDependencyGraph& graph);

/// Normalize a virtual or relative path for equality (lowercase, `/`, no `./`).
eastl::string normalizeWatchVirtualPath(const eastl::string& path);

}  // namespace Blunder
