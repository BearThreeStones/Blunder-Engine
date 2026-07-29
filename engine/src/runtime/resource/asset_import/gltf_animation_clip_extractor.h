#pragma once

#include <filesystem>
#include <functional>

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

/// Parse animations from a glTF/GLB Intermediate file, write clip YAML +
/// AnimationClip Asset descriptors under assets/Animations/, and register GUIDs.
/// Returns one ImportResult per successfully registered clip (may be empty).
eastl::vector<ImportResult> extractAndRegisterAnimationClipsFromGltf(
    FileSystem* file_system, AssetRegistry* asset_registry,
    ContentBrowserSystem* content_browser,
    const std::filesystem::path& gltf_absolute, const eastl::string& mesh_stem,
    const MakeUniqueDescriptorNameFn& make_unique_descriptor_name);

}  // namespace Blunder
