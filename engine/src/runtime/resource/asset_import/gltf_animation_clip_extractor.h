#pragma once

#include <filesystem>
#include <functional>

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset_import/asset_import_service.h"

namespace Blunder {

class AssetRegistry;
class ContentBrowserSystem;
class FileSystem;

using MakeUniqueDescriptorNameFn =
    std::function<eastl::string(const eastl::string& folder,
                                const eastl::string& stem, const char* suffix)>;

/// Binding for a clip already registered under resources/Animations/<meshStem>/.
struct ExistingAnimationClipBinding {
  eastl::string guid;
  eastl::string intermediate_virtual;
  eastl::string descriptor_virtual;
};

using ExistingAnimationClipMap =
    eastl::hash_map<eastl::string, ExistingAnimationClipBinding>;

/// Collect clip bindings for a mesh stem by scanning registry entries whose
/// descriptor source points at resources/Animations/<meshStem>/.
ExistingAnimationClipMap collectExistingAnimationClipsForMesh(
    FileSystem* file_system, AssetRegistry* asset_registry,
    const eastl::string& mesh_stem);

/// Parse animations from a glTF/GLB Intermediate file, write clip YAML +
/// AnimationClip Asset descriptors under `assets_folder_virtual`, and register
/// GUIDs. When `preferred_clip_stem` is non-empty, use it for the first clip
/// and append numeric suffixes for additional animations in the same file.
/// Returns one ImportResult per successfully registered clip (may be empty).
eastl::vector<ImportResult> extractAndRegisterAnimationClipsFromGltf(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser,
    const std::filesystem::path& gltf_absolute, const eastl::string& mesh_stem,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name,
    const eastl::string& preferred_clip_stem = {},
    const eastl::string& assets_folder_virtual = "assets/Animations");

/// Re-extract clip YAML from glTF, reusing GUIDs for stable clip names.
/// New animations create new clips under `assets_folder_virtual`; removed
/// animations leave orphan descriptors.
eastl::vector<ImportResult> refreshAnimationClipsFromGltf(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser,
    const std::filesystem::path& gltf_absolute, const eastl::string& mesh_stem,
    const ExistingAnimationClipMap& existing_clips,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name,
    const eastl::string& preferred_clip_stem = {},
    const eastl::string& assets_folder_virtual = "assets/Animations");

}  // namespace Blunder
