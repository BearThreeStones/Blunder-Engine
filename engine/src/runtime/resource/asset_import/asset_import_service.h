#pragma once

#include <filesystem>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

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

  static bool isMeshSourceExtension(const eastl::string& extension_lower);
  static bool isTextureSourceExtension(const eastl::string& extension_lower);

 private:
  eastl::string makeUniqueDescriptorName(const eastl::string& folder,
                                         const eastl::string& stem,
                                         const char* suffix) const;
  bool copyGltfWithSidecars(const std::filesystem::path& source_absolute,
                            const std::filesystem::path& dest_directory,
                            eastl::string& out_resource_virtual_path);

  FileSystem* m_file_system{nullptr};
  AssetRegistry* m_asset_registry{nullptr};
  ContentBrowserSystem* m_content_browser{nullptr};
  bool m_is_initialized{false};
};

}  // namespace Blunder
