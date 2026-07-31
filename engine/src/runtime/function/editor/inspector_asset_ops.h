#pragma once

#include "EASTL/string.h"

namespace Blunder {

class AssetRegistry;
class FileSystem;

struct AssetInspectorIdentity {
  eastl::string display_name;
  eastl::string guid;
  eastl::string type_label;
  eastl::string intermediate_path;
};

/// True for Content Browser Mesh descriptor paths (e.g. assets/Meshes/foo.mesh.yaml).
bool isMeshAssetDescriptorPath(const eastl::string& virtual_path);

/// Basename for UI display (e.g. Sponza.mesh.yaml).
eastl::string meshAssetDisplayNameFromPath(const eastl::string& virtual_path);

/// Fills read-only Asset Inspector identity for a Mesh descriptor virtual path.
bool resolveMeshAssetInspectorIdentity(
    const eastl::string& descriptor_virtual_path, const AssetRegistry* registry,
    FileSystem* file_system, AssetInspectorIdentity& out_identity);

}  // namespace Blunder
