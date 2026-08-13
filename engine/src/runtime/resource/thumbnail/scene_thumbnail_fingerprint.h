#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

class AssetRegistry;
class FileSystem;

/// Collect direct Mesh Asset References (GUID or legacy path) from a scene file
/// Collects direct Mesh Asset References on a single Scene Asset (legacy
/// nested childScenes are ignored). Does not expand texture/material deps.
eastl::vector<eastl::string> collectSceneDirectMeshReferences(
    FileSystem& file_system, AssetRegistry* asset_registry,
    const eastl::string& scene_virtual_path);

/// Stable fingerprint string for Scene Thumbnail cache invalidation:
/// sorted unique mesh refs + their descriptor (or source) mtimes, plus scene mtime.
eastl::string computeSceneThumbnailFingerprint(
    FileSystem& file_system, AssetRegistry* asset_registry,
    const eastl::string& scene_virtual_path, uint64_t scene_mtime);

}  // namespace Blunder
