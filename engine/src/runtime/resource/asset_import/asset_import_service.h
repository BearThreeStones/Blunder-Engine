#pragma once

#include <cstdint>
#include <filesystem>

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
};

struct AssetImportServiceInit {
  FileSystem* file_system{nullptr};
  AssetRegistry* asset_registry{nullptr};
  ContentBrowserSystem* content_browser{nullptr};
  AssetCompilerService* asset_compiler{nullptr};
};

/// Registers Intermediate exchange files (COLLADA / images) as Assets, and runs
/// Assimp Source Export for FBX/OBJ/glTF/GLB (dual-write Source archive +
/// Intermediate). Descriptor field `source` stores the Intermediate virtual
/// path; Source Export also sets `archived_source` to the Resources/Source
/// archive path.
class AssetImportService final {
 public:
  void initialize(const AssetImportServiceInit& init);
  void shutdown();

  /// Import a mesh: COLLADA Intermediate register, or FBX/OBJ/glTF/GLB Source
  /// Export (archive under Resources/Source/, Assimp → Intermediate COLLADA
  /// under Models/).
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
  /// Intermediate COLLADA (overwrite), then invalidates Finals/dependents.
  /// Intermediate-only: invalidate Finals (settings refresh optional / no
  /// GUID change). Rebuilds the dependency graph once (equivalent to
  /// requestReimports of one).
  bool requestReimport(const eastl::string& guid);

  /// Batch Reimport: one rebuildDependencyGraph, then refresh + invalidate
  /// each GUID. Prefer this over N× requestReimport for watch debounce flush.
  bool requestReimports(const eastl::vector<eastl::string>& guids);

  /// Lazy Intermediate Upgrade (project open / registry scan): for each mesh
  /// Asset whose Intermediate `source` is still glTF/GLB, Assimp-convert to a
  /// sibling `.dae`, rewrite `source`, archive the former file under Source
  /// when `archived_source` is empty, preserve GUID, and mark Finals stale.
  /// Fail-soft: conversion failure leaves descriptor/`source` unchanged.
  /// Returns the number of Assets successfully upgraded.
  uint32_t upgradeLegacyMeshIntermediates();

  /// Rebuild registry from Assets/ scan, then run Intermediate Upgrade.
  /// Editor/tests use this as the combined scan/open path.
  uint32_t scanAndUpgradeLegacyIntermediates();

  /// Test seam: when true, Intermediate Upgrade convert always fails so fail-soft
  /// behavior can be asserted with a still-loadable legacy glTF Intermediate.
  static void setForceUpgradeConvertFailureForTest(bool force);

  /// COLLADA Intermediate exchange extension (not Source Assets).
  static bool isMeshIntermediateExtension(const eastl::string& extension_lower);
  /// Image Intermediate exchange extensions (not Source Assets).
  static bool isTextureIntermediateExtension(
      const eastl::string& extension_lower);
  /// FBX/OBJ/glTF/GLB whitelist for Assimp Source Export (v1).
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
      const MeshImportSettings& settings);
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
