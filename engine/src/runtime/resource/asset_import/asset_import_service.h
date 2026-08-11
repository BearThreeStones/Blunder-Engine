#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AssetCompilerService;
class AssetRegistry;
class ContentBrowserSystem;
class FileSystem;

struct ImportResult {
  eastl::string descriptor_virtual_path;
  eastl::string guid;
  bool success{false};
  /// AnimationClip Assets extracted alongside a mesh glTF Import (Task 2.3).
  eastl::vector<ImportResult> animation_clips;
  /// Persisted companion glTF/GLB Intermediate bodies paired with this Mesh.
  std::vector<std::filesystem::path> companion_animation_paths;
};

struct AssetImportServiceInit {
  FileSystem* file_system{nullptr};
  AssetRegistry* asset_registry{nullptr};
  ContentBrowserSystem* content_browser{nullptr};
  AssetCompilerService* asset_compiler{nullptr};
};

/// Registers Intermediate exchange files (glTF/GLB / images) as Assets, and runs
/// Assimp Source Export for FBX/OBJ (dual-write Source archive + Intermediate
/// glTF). Descriptor field `source` stores the Intermediate virtual path;
/// Source Export also sets `archived_source` to the Resources/Source archive
/// path.
class AssetImportService final {
 public:
  void initialize(const AssetImportServiceInit& init);
  void shutdown();

  /// Import a mesh: glTF/GLB Intermediate register, or FBX/OBJ Source Export
  /// (archive under Resources/Source/, Assimp → Intermediate glTF under
  /// Models/).
  ImportResult importMesh(const std::filesystem::path& input_absolute,
                          const eastl::string& assets_folder_virtual,
                          const MeshImportSettings& settings);

  /// Import an image Intermediate file: copy under Resources/Textures/{name}/
  /// (when not already a non-Source Resources path) and write a .texture.yaml.
  ImportResult importTexture(const std::filesystem::path& input_absolute,
                             const eastl::string& assets_folder_virtual,
                             const TextureImportSettings& settings);

  eastl::vector<ImportResult> importExternalFiles(
      const eastl::vector<eastl::string>& absolute_paths,
      const eastl::string& assets_folder_virtual,
      const MeshImportSettings& mesh_settings = {});

  /// Find Assets whose descriptor `archived_source` matches this absolute
  /// SourceArchive path (Resources/Source/...).
  eastl::vector<eastl::string> findGuidsByArchivedSource(
      const std::filesystem::path& absolute_source_path) const;

  /// Request Reimport for an Asset GUID. Preserves GUID always.
  /// If `archived_source` is Source Export whitelist: Assimp re-exports
  /// Intermediate glTF/GLB (overwrite), then invalidates Finals/dependents.
  /// Intermediate-only: invalidate Finals (settings refresh optional / no
  /// GUID change). Rebuilds the dependency graph once (equivalent to
  /// requestReimports of one).
  bool requestReimport(const eastl::string& guid);

  /// Batch Reimport: one rebuildDependencyGraph, then refresh + invalidate
  /// each GUID. Prefer this over N× requestReimport for watch debounce flush.
  bool requestReimports(const eastl::vector<eastl::string>& guids);

  /// Delete a registered Asset by descriptor virtual path (e.g. assets/Meshes/X.mesh.yaml).
  /// Refuses when the dependency graph reports dependents. On success: removes
  /// Intermediate `source` (and Mesh companion Intermediate bodies), descriptor,
  /// unregisters GUID, marks Finals stale, refreshes Content Browser.
  /// `out_error` receives a short reason when returning false.
  bool deleteAsset(const eastl::string& descriptor_virtual_path,
                   eastl::string* out_error = nullptr);

  /// Lazy Intermediate migration (project open / registry scan): for each mesh
  /// Asset whose Intermediate `source` is still `.dae`, migrate GUID-preserving
  /// to sibling glTF/GLB (Assimp convert) or Reimport from `archived_source`
  /// when Source Export whitelist. glTF/GLB sources are never downgraded to
  /// COLLADA. Fail-soft: conversion failure leaves descriptor/`source` as `.dae`.
  /// Returns the number of Assets successfully migrated.
  uint32_t upgradeLegacyMeshIntermediates();

  /// Rebuild registry from Assets/ scan, then run Intermediate migration.
  uint32_t scanAndUpgradeLegacyIntermediates();

  /// Test seam: when true, Intermediate migration convert always fails so
  /// fail-soft behavior can be asserted with a still-loadable legacy `.dae`.
  static void setForceUpgradeConvertFailureForTest(bool force);

  /// glTF/GLB Intermediate exchange extensions (not Source Assets).
  static bool isMeshIntermediateExtension(const eastl::string& extension_lower);
  /// Image Intermediate exchange extensions (not Source Assets).
  static bool isTextureIntermediateExtension(
      const eastl::string& extension_lower);
  /// FBX/OBJ whitelist for Assimp Source Export (v1).
  static bool isMeshSourceExportExtension(const eastl::string& extension_lower);

  /// Deprecated aliases — prefer Intermediate names above.
  static bool isMeshSourceExtension(const eastl::string& extension_lower) {
    return isMeshIntermediateExtension(extension_lower);
  }
  static bool isTextureSourceExtension(const eastl::string& extension_lower) {
    return isTextureIntermediateExtension(extension_lower);
  }

 private:
  ImportResult importMeshIntermediate(
      const std::filesystem::path& input_absolute,
      const eastl::string& assets_folder_virtual,
      const MeshImportSettings& settings,
      const std::vector<std::filesystem::path>& companion_animation_paths = {});
  ImportResult importMeshSourceExport(
      const std::filesystem::path& input_absolute,
      const eastl::string& assets_folder_virtual,
      const MeshImportSettings& settings);

  eastl::string makeUniqueDescriptorName(const eastl::string& folder,
                                         const eastl::string& stem,
                                         const char* suffix) const;

  FileSystem* m_file_system{nullptr};
  AssetRegistry* m_asset_registry{nullptr};
  ContentBrowserSystem* m_content_browser{nullptr};
  AssetCompilerService* m_asset_compiler{nullptr};
  bool m_is_initialized{false};
};

}  // namespace Blunder
