#pragma once

#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

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

bool loadMeshAssetDescriptor(const eastl::string& descriptor_virtual_path,
                             FileSystem* file_system,
                             MeshAssetDescriptor& out_descriptor);

bool saveMeshAssetDescriptor(const eastl::string& descriptor_virtual_path,
                             FileSystem* file_system,
                             const MeshAssetDescriptor& descriptor);

/// Content Browser selection enters Asset Inspector only for Mesh descriptors.
inline bool shouldEnterAssetInspectorForBrowserPath(
    const eastl::string& virtual_path) {
  return isMeshAssetDescriptorPath(virtual_path);
}

/// Browser Mesh selection should clear scene entity selection.
inline bool shouldClearEntitySelectionForBrowserAssetPath(
    const eastl::string& virtual_path) {
  return shouldEnterAssetInspectorForBrowserPath(virtual_path);
}

/// Scene entity selection should exit Asset Inspector when it is active.
inline bool shouldExitAssetInspectorOnEntitySelection(bool inspector_asset_mode) {
  return inspector_asset_mode;
}

/// Mesh Preview captures pointer input while Asset Inspector is active.
inline bool inspectorMeshPreviewPointerCaptureActive(bool asset_mode) {
  return asset_mode;
}

}  // namespace Blunder
