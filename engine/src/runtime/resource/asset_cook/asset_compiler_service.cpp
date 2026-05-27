#include "runtime/resource/asset_cook/asset_compiler_service.h"

#include <cstring>
#include <filesystem>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_cook/texture_cooker.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

bool endsWith(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

fs::path resolveDescriptorAbsolute(FileSystem& file_system,
                                 const eastl::string& virtual_path) {
  eastl::string relative = virtual_path;
  if (relative.compare(0, 7, "assets/") == 0) {
    relative.erase(0, 7);
  }
  return file_system.resolveAsset(fs::path(relative.c_str()));
}

fs::path resolveSourceAbsolute(FileSystem& file_system,
                               const eastl::string& virtual_path) {
  eastl::string relative = virtual_path;
  if (relative.compare(0, 10, "resources/") == 0) {
    relative.erase(0, 10);
  } else if (relative.compare(0, 7, "assets/") == 0) {
    relative.erase(0, 7);
    return file_system.resolveAsset(fs::path(relative.c_str()));
  }
  return file_system.resolveResource(fs::path(relative.c_str()));
}

bool isCookFresh(FileSystem& file_system, const fs::path& cooked_path,
                 const fs::path& meta_path, uint64_t source_mtime,
                 uint64_t descriptor_mtime) {
  if (!file_system.exists(cooked_path) || !file_system.exists(meta_path)) {
    return false;
  }

  CookedAssetMeta meta{};
  if (!readCookMetaFile(meta_path, meta)) {
    return false;
  }
  return meta.source_mtime == source_mtime &&
         meta.descriptor_mtime == descriptor_mtime;
}

}  // namespace

void AssetCompilerService::initialize(FileSystem* file_system,
                                      AssetManager* asset_manager,
                                      AssetRegistry* asset_registry) {
  m_file_system = file_system;
  m_asset_manager = asset_manager;
  m_asset_registry = asset_registry;
  m_is_initialized =
      file_system != nullptr && asset_manager != nullptr && asset_registry != nullptr;
}

void AssetCompilerService::shutdown() {
  m_file_system = nullptr;
  m_asset_manager = nullptr;
  m_asset_registry = nullptr;
  m_is_initialized = false;
}

AssetCompilerStats AssetCompilerService::cookAll(bool force) {
  AssetCompilerStats stats{};
  if (!m_is_initialized) {
    return stats;
  }

  m_asset_registry->rebuildFromScan();

  const fs::path asset_root = m_file_system->getAssetRoot();
  const eastl::vector<DirectoryEntry> entries =
      m_file_system->listDirectoryRecursive(asset_root, asset_root, -1);

  for (const DirectoryEntry& entry : entries) {
    if (entry.is_directory) {
      continue;
    }

    const eastl::string virtual_path(
        (eastl::string("assets/") + entry.relative_path.c_str()).c_str());
    bool cooked = false;
    if (endsWith(virtual_path, ".mesh.yaml")) {
      cooked = cookMeshDescriptor(virtual_path, force);
      if (cooked) {
        ++stats.meshes_cooked;
      } else if (m_file_system->exists(entry.absolute_path)) {
        ++stats.skipped;
      } else {
        ++stats.failed;
      }
    } else if (endsWith(virtual_path, ".texture.yaml")) {
      cooked = cookTextureDescriptor(virtual_path, force);
      if (cooked) {
        ++stats.textures_cooked;
      } else if (m_file_system->exists(entry.absolute_path)) {
        ++stats.skipped;
      } else {
        ++stats.failed;
      }
    }
    (void)cooked;
  }

  LOG_INFO(
      "[AssetCompiler] cooked meshes={} textures={} skipped={} failed={}",
      stats.meshes_cooked, stats.textures_cooked, stats.skipped, stats.failed);
  return stats;
}

AssetCompilerStats AssetCompilerService::cookIfStale() {
  return cookAll(false);
}

bool AssetCompilerService::cookMeshDescriptor(
    const eastl::string& descriptor_virtual_path, bool force) {
  const fs::path descriptor_absolute =
      resolveDescriptorAbsolute(*m_file_system, descriptor_virtual_path);

  eastl::string yaml_text;
  if (!m_file_system->readText(descriptor_absolute, yaml_text)) {
    return false;
  }

  MeshAssetDescriptor descriptor{};
  if (!AssetYaml::parseMeshDescriptor(yaml_text, descriptor)) {
    return false;
  }

  const fs::path source_absolute =
      resolveSourceAbsolute(*m_file_system, descriptor.source);
  const uint64_t source_mtime = m_file_system->lastWriteTime(source_absolute);
  const uint64_t descriptor_mtime =
      m_file_system->lastWriteTime(descriptor_absolute);

  const fs::path cooked_path = cookedMeshPath(*m_file_system, descriptor.guid);
  const fs::path meta_path = cookedMeshMetaPath(*m_file_system, descriptor.guid);

  if (!force &&
      isCookFresh(*m_file_system, cooked_path, meta_path, source_mtime,
                  descriptor_mtime)) {
    return false;
  }

  const eastl::shared_ptr<MeshAsset> mesh =
      m_asset_manager->loadMesh(descriptor_virtual_path);
  if (!mesh) {
    LOG_ERROR("[AssetCompiler] failed to load mesh descriptor {}",
              descriptor_virtual_path.c_str());
    return false;
  }

  m_file_system->ensureParentDirectory(cooked_path);
  if (!writeMeshCookFile(cooked_path, mesh->getVertices(), mesh->getIndices())) {
    return false;
  }

  CookedAssetMeta meta{};
  meta.source_mtime = source_mtime;
  meta.descriptor_mtime = descriptor_mtime;
  writeCookMetaFile(meta_path, meta);

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual_path);
  LOG_INFO("[AssetCompiler] cooked mesh {} -> {}", descriptor_virtual_path.c_str(),
           cooked_path.generic_string());
  return true;
}

bool AssetCompilerService::cookTextureDescriptor(
    const eastl::string& descriptor_virtual_path, bool force) {
  const fs::path descriptor_absolute =
      resolveDescriptorAbsolute(*m_file_system, descriptor_virtual_path);

  eastl::string yaml_text;
  if (!m_file_system->readText(descriptor_absolute, yaml_text)) {
    return false;
  }

  TextureAssetDescriptor descriptor{};
  if (!AssetYaml::parseTextureDescriptor(yaml_text, descriptor)) {
    return false;
  }

  const fs::path source_absolute =
      resolveSourceAbsolute(*m_file_system, descriptor.source);
  const uint64_t source_mtime = m_file_system->lastWriteTime(source_absolute);
  const uint64_t descriptor_mtime =
      m_file_system->lastWriteTime(descriptor_absolute);

  const fs::path cooked_path =
      cookedTexturePath(*m_file_system, descriptor.guid);
  const fs::path meta_path =
      cookedTextureMetaPath(*m_file_system, descriptor.guid);

  if (!force &&
      isCookFresh(*m_file_system, cooked_path, meta_path, source_mtime,
                  descriptor_mtime)) {
    return false;
  }

  const eastl::shared_ptr<Texture2DAsset> texture =
      m_asset_manager->loadTexture2D(descriptor.source);
  if (!texture) {
    LOG_ERROR("[AssetCompiler] failed to load texture source {}",
              descriptor.source.c_str());
    return false;
  }

  m_file_system->ensureParentDirectory(cooked_path);
  if (!writeTextureCookFile(cooked_path, texture->getWidth(),
                            texture->getHeight(), texture->getPixelData(),
                            descriptor.import.srgb)) {
    return false;
  }

  CookedAssetMeta meta{};
  meta.source_mtime = source_mtime;
  meta.descriptor_mtime = descriptor_mtime;
  writeCookMetaFile(meta_path, meta);

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual_path);
  LOG_INFO("[AssetCompiler] cooked texture {} -> {}",
           descriptor_virtual_path.c_str(), cooked_path.generic_string());
  return true;
}

}  // namespace Blunder
