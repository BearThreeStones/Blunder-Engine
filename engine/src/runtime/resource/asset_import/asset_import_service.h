#pragma once

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

/// Registers Intermediate exchange files (glTF / images) as Assets, and runs
/// Assimp Source Export for FBX/OBJ (dual-write Source archive + Intermediate).
/// Descriptor field `source` stores the Intermediate virtual path; Source Export
/// also sets `archived_source` to the Resources/Source archive path.
class AssetImportService final {
 public:
  void initialize(const AssetImportServiceInit& init);
  void shutdown();

  /// Import a mesh: glTF/GLB Intermediate register, or FBX/OBJ Source Export
  /// (archive under Resources/Source/, Assimp → Intermediate glTF under Models/).
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

  /// Request Reimport for an Asset GUID. v1 stub (task 4.4): logs and
  /// invalidateAssetAndDependents when a compiler is wired. Full Assimp
  /// Source Export re-run lands in task 5.3.
  /// Rebuilds the dependency graph once (equivalent to requestReimports of one).
  bool requestReimport(const eastl::string& guid);

  /// Batch Reimport: one rebuildDependencyGraph, then invalidate each GUID.
  /// Prefer this over N× requestReimport when applying a watch debounce flush.
  bool requestReimports(const eastl::vector<eastl::string>& guids);

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
