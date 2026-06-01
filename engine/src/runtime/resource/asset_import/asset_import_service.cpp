#include "runtime/resource/asset_import/asset_import_service.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset_registry/asset_registry.h"
#include "runtime/resource/content_browser/content_browser_system.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

eastl::string extensionLower(const fs::path& path) {
  eastl::string ext(path.extension().generic_string().c_str());
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return ext;
}

eastl::string joinVirtualPath(const eastl::string& folder,
                              const eastl::string& file_name) {
  eastl::string result = folder;
  if (!result.empty() && result.back() != '/') {
    result.push_back('/');
  }
  result.append(file_name);
  return result;
}

eastl::string resolveAssetsFolder(const eastl::string& selected_folder) {
  eastl::string folder = selected_folder;
  if (folder.compare(0, 10, "resources/") == 0) {
    folder.replace(0, 10, "assets/");
  }
  if (folder.empty()) {
    folder = "assets/";
  }
  return folder;
}

bool relativePathEscapesRoot(const std::filesystem::path& relative_path) {
  for (const auto& part : relative_path.lexically_normal()) {
    if (part == ".") {
      continue;
    }
    return part == "..";
  }
  return false;
}

eastl::string resolveSourceVirtualPath(FileSystem* file_system, const fs::path& source_absolute) {
  std::error_code ec;
  const fs::path resources_root = file_system->getResourcesRoot();
  const fs::path relative = fs::relative(source_absolute, resources_root, ec);
  if (!ec && !relative.empty() && !relativePathEscapesRoot(relative)) {
    eastl::string virtual_path("resources/");
    virtual_path.append(relative.generic_string().c_str());
    return virtual_path;
  }
  return eastl::string(source_absolute.generic_string().c_str());
}

}  // namespace

void AssetImportService::initialize(const AssetImportServiceInit& init) {
  m_file_system = init.file_system;
  m_asset_registry = init.asset_registry;
  m_content_browser = init.content_browser;
  m_is_initialized = m_file_system != nullptr && m_asset_registry != nullptr;
}

void AssetImportService::shutdown() {
  m_file_system = nullptr;
  m_asset_registry = nullptr;
  m_content_browser = nullptr;
  m_is_initialized = false;
}

bool AssetImportService::isMeshSourceExtension(
    const eastl::string& extension_lower) {
  return extension_lower == ".gltf" || extension_lower == ".glb";
}

bool AssetImportService::isTextureSourceExtension(
    const eastl::string& extension_lower) {
  return extension_lower == ".png" || extension_lower == ".jpg" ||
         extension_lower == ".jpeg" || extension_lower == ".bmp" ||
         extension_lower == ".tga";
}

eastl::string AssetImportService::makeUniqueDescriptorName(
    const eastl::string& folder, const eastl::string& stem,
    const char* suffix) const {
  auto name_exists = [&](const eastl::string& file_name) -> bool {
    const eastl::string virtual_path = joinVirtualPath(folder, file_name);
    if (m_content_browser && m_content_browser->findEntry(virtual_path)) {
      return true;
    }
    eastl::string relative = folder;
    if (relative.compare(0, 7, "assets/") == 0) {
      relative.erase(0, 7);
    }
    const fs::path absolute =
        m_file_system->resolveAsset(fs::path(relative.c_str()) / file_name.c_str());
    return m_file_system->exists(absolute);
  };

  eastl::string file_name = stem;
  file_name.append(suffix);
  if (!name_exists(file_name)) {
    return file_name;
  }

  for (uint32_t index = 1; index < 10000; ++index) {
    char alt[128];
    std::snprintf(alt, sizeof(alt), "%s_%u%s", stem.c_str(), index, suffix);
    if (!name_exists(eastl::string(alt))) {
      return eastl::string(alt);
    }
  }
  return eastl::string();
}

ImportResult AssetImportService::importMesh(
    const fs::path& source_absolute, const eastl::string& assets_folder_virtual,
    const MeshImportSettings& settings) {
  ImportResult result{};
  if (!m_is_initialized || assets_folder_virtual.empty()) {
    return result;
  }

  const eastl::string ext = extensionLower(source_absolute);
  if (!isMeshSourceExtension(ext)) {
    LOG_WARN("[AssetImport] unsupported mesh source {}", source_absolute.generic_string());
    return result;
  }

  const eastl::string assets_folder = resolveAssetsFolder(assets_folder_virtual);
  const eastl::string resource_virtual_path = resolveSourceVirtualPath(m_file_system, source_absolute);

  const eastl::string stem(source_absolute.stem().generic_string().c_str());
  const eastl::string descriptor_name =
      makeUniqueDescriptorName(assets_folder, stem, ".mesh.yaml");
  if (descriptor_name.empty()) {
    LOG_WARN("[AssetImport] descriptor already exists for mesh {}", stem.c_str());
    return result;
  }

  MeshAssetDescriptor descriptor{};
  descriptor.guid = m_asset_registry->allocateGuid();
  descriptor.source = resource_virtual_path;
  descriptor.import = settings;

  const eastl::string descriptor_virtual =
      joinVirtualPath(assets_folder, descriptor_name);
  eastl::string relative = descriptor_virtual;
  relative.erase(0, 7);
  const fs::path descriptor_absolute =
      m_file_system->resolveAsset(fs::path(relative.c_str()));

  m_file_system->ensureParentDirectory(descriptor_absolute);
  if (!m_file_system->writeText(descriptor_absolute,
                                AssetYaml::serializeMeshDescriptor(descriptor))) {
    return result;
  }

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual);

  result.descriptor_virtual_path = descriptor_virtual;
  result.guid = descriptor.guid;
  result.success = true;

  LOG_INFO("[AssetImport] mesh {} -> {} (source: {})",
           source_absolute.generic_string(), descriptor_virtual.c_str(),
           resource_virtual_path.c_str());
  return result;
}

ImportResult AssetImportService::importTexture(
    const fs::path& source_absolute, const eastl::string& assets_folder_virtual,
    const TextureImportSettings& settings) {
  ImportResult result{};
  if (!m_is_initialized || assets_folder_virtual.empty()) {
    return result;
  }

  const eastl::string ext = extensionLower(source_absolute);
  if (!isTextureSourceExtension(ext)) {
    LOG_WARN("[AssetImport] unsupported texture source {}",
             source_absolute.generic_string());
    return result;
  }

  const eastl::string assets_folder = resolveAssetsFolder(assets_folder_virtual);
  const eastl::string resource_virtual_path = resolveSourceVirtualPath(m_file_system, source_absolute);

  const eastl::string stem(source_absolute.stem().generic_string().c_str());
  const eastl::string descriptor_name =
      makeUniqueDescriptorName(assets_folder, stem, ".texture.yaml");
  if (descriptor_name.empty()) {
    return result;
  }

  TextureAssetDescriptor descriptor{};
  descriptor.guid = m_asset_registry->allocateGuid();
  descriptor.source = resource_virtual_path;
  descriptor.import = settings;

  const eastl::string descriptor_virtual =
      joinVirtualPath(assets_folder, descriptor_name);
  eastl::string relative = descriptor_virtual;
  relative.erase(0, 7);
  const fs::path descriptor_absolute =
      m_file_system->resolveAsset(fs::path(relative.c_str()));

  m_file_system->ensureParentDirectory(descriptor_absolute);
  if (!m_file_system->writeText(descriptor_absolute,
                                AssetYaml::serializeTextureDescriptor(descriptor))) {
    return result;
  }

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual);

  result.descriptor_virtual_path = descriptor_virtual;
  result.guid = descriptor.guid;
  result.success = true;

  LOG_INFO("[AssetImport] texture {} -> {} (source: {})",
           source_absolute.generic_string(), descriptor_virtual.c_str(),
           resource_virtual_path.c_str());
  return result;
}

eastl::vector<ImportResult> AssetImportService::importExternalFiles(
    const eastl::vector<eastl::string>& absolute_paths,
    const eastl::string& assets_folder_virtual,
    const MeshImportSettings& mesh_settings) {
  eastl::vector<ImportResult> results;
  if (!m_is_initialized) {
    return results;
  }

  eastl::vector<eastl::string> pending_meshes;
  for (const eastl::string& absolute_path : absolute_paths) {
    const fs::path src(absolute_path.c_str());
    const eastl::string ext = extensionLower(src);
    if (isMeshSourceExtension(ext)) {
      pending_meshes.push_back(absolute_path);
      continue;
    }
    if (isTextureSourceExtension(ext)) {
      TextureImportSettings texture_settings{};
      ImportResult imported =
          importTexture(src, assets_folder_virtual, texture_settings);
      if (imported.success) {
        results.push_back(imported);
      }
    }
  }

  for (const eastl::string& absolute_path : pending_meshes) {
    ImportResult imported =
        importMesh(fs::path(absolute_path.c_str()), assets_folder_virtual,
                   mesh_settings);
    if (imported.success) {
      results.push_back(imported);
    }
  }

  if (!results.empty() && m_content_browser) {
    m_content_browser->refresh();
  }
  return results;
}

}  // namespace Blunder
