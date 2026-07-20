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

class AssetImportService final {
 public:
  void initialize(const AssetImportServiceInit& init);
  void shutdown();

  ImportResult importMesh(const std::filesystem::path& source_absolute,
                          const eastl::string& assets_folder_virtual,
                          const MeshImportSettings& settings);
  ImportResult importTexture(const std::filesystem::path& source_absolute,
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

  static bool isMeshSourceExtension(const eastl::string& extension_lower);
  static bool isTextureSourceExtension(const eastl::string& extension_lower);

 private:
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
