#include "runtime/resource/asset_cook/asset_compiler_service.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "EASTL/hash_set.h"
#include "EASTL/vector.h"

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/asset/asset_yaml.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/texture2d_asset.h"
#include "runtime/resource/asset_cook/mesh_cooker.h"
#include "runtime/resource/asset_cook/meshlet_builder.h"
#include "runtime/resource/asset_cook/texture_cooker.h"
#include "runtime/resource/asset_import/asset_import_service.h"
#include "runtime/resource/asset_manager/asset_manager.h"
#include "runtime/resource/asset_registry/asset_registry.h"

namespace Blunder {

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kCookStampVersion = 1;

bool endsWith(const eastl::string& value, const char* suffix) {
  const size_t suffix_length = std::strlen(suffix);
  if (value.size() < suffix_length) {
    return false;
  }
  return value.compare(value.size() - suffix_length, suffix_length, suffix) ==
         0;
}

bool endsWithPath(const fs::path& path, const char* suffix) {
  const std::string name = path.filename().generic_string();
  const size_t suffix_length = std::strlen(suffix);
  if (name.size() < suffix_length) {
    return false;
  }
  return name.compare(name.size() - suffix_length, suffix_length, suffix) == 0;
}

fs::path cookStampPath(FileSystem& file_system) {
  return cookedRoot(file_system) / "cook_stamp";
}

struct CookStampFingerprint {
  uint32_t descriptor_count{0};
  uint64_t hash{14695981039346656037ull};  // FNV-1a 64 offset
};

void fnv1aMix(uint64_t& hash, const char* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
    hash *= 1099511628211ull;
  }
}

void fnv1aMixU64(uint64_t& hash, uint64_t value) {
  char buf[32];
  const int written =
      std::snprintf(buf, sizeof(buf), "%llu",
                    static_cast<unsigned long long>(value));
  if (written > 0) {
    fnv1aMix(hash, buf, static_cast<size_t>(written));
  }
}

bool isCookDescriptorPath(const fs::path& path) {
  return endsWithPath(path, ".mesh.yaml") || endsWithPath(path, ".texture.yaml");
}

CookStampFingerprint computeCookStampFingerprint(FileSystem& file_system) {
  CookStampFingerprint out{};
  const fs::path asset_root = file_system.getAssetRoot();
  if (!file_system.exists(asset_root)) {
    return out;
  }

  const eastl::vector<DirectoryEntry> entries =
      file_system.listDirectoryRecursive(asset_root, asset_root, -1);

  struct DescriptorSample {
    std::string relative;
    uint64_t mtime{0};
  };
  std::vector<DescriptorSample> samples;
  samples.reserve(256);
  for (const DirectoryEntry& entry : entries) {
    if (entry.is_directory || !isCookDescriptorPath(entry.absolute_path)) {
      continue;
    }
    DescriptorSample sample;
    sample.relative = entry.relative_path.c_str();
    sample.mtime = file_system.lastWriteTime(entry.absolute_path);
    samples.push_back(std::move(sample));
  }

  std::sort(samples.begin(), samples.end(),
            [](const DescriptorSample& a, const DescriptorSample& b) {
              return a.relative < b.relative;
            });

  for (const DescriptorSample& sample : samples) {
    fnv1aMix(out.hash, sample.relative.c_str(), sample.relative.size());
    const char nul = '\0';
    fnv1aMix(out.hash, &nul, 1);
    fnv1aMixU64(out.hash, sample.mtime);
    ++out.descriptor_count;
  }
  return out;
}

void removeIfExists(FileSystem& file_system, const fs::path& path) {
  if (!file_system.exists(path)) {
    return;
  }
  std::error_code ec;
  fs::remove(path, ec);
  if (ec) {
    LOG_WARN("[AssetCompiler] failed to remove {}: {}", path.generic_string(),
             ec.message());
  }
}

bool readCookStamp(FileSystem& file_system, CookStampFingerprint& out) {
  const fs::path path = cookStampPath(file_system);
  if (!file_system.exists(path)) {
    return false;
  }
  std::ifstream stream(path);
  if (!stream) {
    return false;
  }

  uint32_t version = 0;
  uint32_t descriptor_count = 0;
  uint64_t hash = 0;
  bool found_version = false;
  bool found_count = false;
  bool found_hash = false;
  std::string line;
  while (std::getline(stream, line)) {
    const size_t colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, colon);
    const std::string value = line.substr(colon + 1);
    if (key == "version") {
      version = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
      found_version = true;
    } else if (key == "descriptor_count") {
      descriptor_count =
          static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
      found_count = true;
    } else if (key == "fingerprint") {
      hash = std::strtoull(value.c_str(), nullptr, 16);
      found_hash = true;
    }
  }
  if (!found_version || !found_count || !found_hash ||
      version != kCookStampVersion) {
    return false;
  }
  out.descriptor_count = descriptor_count;
  out.hash = hash;
  return true;
}

bool writeCookStamp(FileSystem& file_system, const CookStampFingerprint& stamp) {
  const fs::path path = cookStampPath(file_system);
  file_system.ensureParentDirectory(path);
  std::ofstream stream(path, std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream << "version: " << kCookStampVersion << '\n';
  stream << "descriptor_count: " << stamp.descriptor_count << '\n';
  char hash_hex[32];
  std::snprintf(hash_hex, sizeof(hash_hex), "%016llx",
                static_cast<unsigned long long>(stamp.hash));
  stream << "fingerprint: " << hash_hex << '\n';
  return stream.good();
}

void removeCookStamp(FileSystem& file_system) {
  removeIfExists(file_system, cookStampPath(file_system));
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

// A cooked Final is fresh when it exists, its meta matches both mtimes, and,
// when `required_cook_format` is non-zero, the meta records that cook format.
// Meta files written before `cook_format` existed read as 0 and therefore
// count as stale for any non-zero requirement, which forces a recook.
bool isCookFresh(FileSystem& file_system, const fs::path& cooked_path,
                 const fs::path& meta_path, uint64_t source_mtime,
                 uint64_t descriptor_mtime,
                 uint32_t required_cook_format = 0) {
  if (!file_system.exists(cooked_path) || !file_system.exists(meta_path)) {
    return false;
  }

  CookedAssetMeta meta{};
  if (!readCookMetaFile(meta_path, meta)) {
    return false;
  }
  if (required_cook_format != 0 &&
      meta.cook_format != required_cook_format) {
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
  m_dependency_graph_rebuild_count = 0;
  m_is_initialized =
      file_system != nullptr && asset_manager != nullptr && asset_registry != nullptr;
}

void AssetCompilerService::shutdown() {
  m_dependency_graph.clear();
  m_dependency_graph_rebuild_count = 0;
  m_file_system = nullptr;
  m_asset_manager = nullptr;
  m_asset_registry = nullptr;
  m_asset_import = nullptr;
  m_cook_heartbeat = {};
  m_is_initialized = false;
}

void AssetCompilerService::setAssetImportService(
    AssetImportService* asset_import) {
  m_asset_import = asset_import;
}

void AssetCompilerService::rebuildDependencyGraph() {
  if (!m_is_initialized) {
    return;
  }
  m_dependency_graph.rebuildFromProject(*m_file_system, *m_asset_registry);
  ++m_dependency_graph_rebuild_count;
}

eastl::vector<eastl::string> AssetCompilerService::dependentsOf(
    const eastl::string& guid) const {
  if (!m_is_initialized || guid.empty()) {
    return {};
  }
  return m_dependency_graph.dependentsOf(guid);
}

AssetCompilerStats AssetCompilerService::cookAll(bool force) {
  AssetCompilerStats stats{};
  if (!m_is_initialized) {
    return stats;
  }

  if (m_cook_heartbeat && !m_cook_heartbeat()) {
    stats.aborted = true;
    return stats;
  }

  m_asset_registry->rebuildFromScan();
  // Registry warm-up: Intermediate Upgrade is a no-op (ADR 0019).
  if (m_asset_import) {
    m_asset_import->upgradeLegacyMeshIntermediates();
  }

  if (m_cook_heartbeat && !m_cook_heartbeat()) {
    stats.aborted = true;
    return stats;
  }

  const fs::path asset_root = m_file_system->getAssetRoot();
  const eastl::vector<DirectoryEntry> entries =
      m_file_system->listDirectoryRecursive(asset_root, asset_root, -1);

  for (const DirectoryEntry& entry : entries) {
    if (m_cook_heartbeat && !m_cook_heartbeat()) {
      stats.aborted = true;
      break;
    }
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
      "[AssetCompiler] cooked meshes={} textures={} skipped={} failed={}{}",
      stats.meshes_cooked, stats.textures_cooked, stats.skipped, stats.failed,
      stats.aborted ? " (aborted)" : "");
  return stats;
}

AssetCompilerStats AssetCompilerService::cookIfStale() {
  // Warm-up only: full Assets/ scan of stale Finals. Prefer cookAsset /
  // markFinalStale for Pull freshness.
  AssetCompilerStats stats{};
  if (!m_is_initialized) {
    return stats;
  }

  const CookStampFingerprint current =
      computeCookStampFingerprint(*m_file_system);
  CookStampFingerprint stamped{};
  if (readCookStamp(*m_file_system, stamped) &&
      stamped.descriptor_count == current.descriptor_count &&
      stamped.hash == current.hash) {
    m_asset_registry->rebuildFromScan();
    stats.skipped = current.descriptor_count;
    stats.used_stamp = true;
    LOG_INFO(
        "[AssetCompiler] cookIfStale skipped (stamp fresh, descriptors={})",
        current.descriptor_count);
    return stats;
  }

  stats = cookAll(false);
  if (!stats.aborted) {
    if (!writeCookStamp(*m_file_system, current)) {
      LOG_WARN("[AssetCompiler] failed to write cook stamp");
    }
  }
  return stats;
}

AssetCompilerStats AssetCompilerService::cookIfStaleForPlayer() {
  AssetCompilerStats stats{};
  if (!m_is_initialized) {
    return stats;
  }

  CookStampFingerprint stamped{};
  if (readCookStamp(*m_file_system, stamped)) {
    // initialize() already loaded `.blunder/asset_registry.yaml`. Skip the
    // Assets/ fingerprint walk and rebuildFromScan — Pull Fast Path still
    // cooks individual stale Finals on demand.
    stats.skipped = stamped.descriptor_count;
    stats.used_stamp = true;
    LOG_INFO(
        "[AssetCompiler] cookIfStaleForPlayer trusted stamp + registry yaml "
        "(descriptors={}, no Assets walk)",
        stamped.descriptor_count);
    return stats;
  }

  LOG_INFO(
      "[AssetCompiler] cookIfStaleForPlayer: no stamp, falling back to "
      "cookIfStale");
  return cookIfStale();
}

void AssetCompilerService::invalidateCookStamp() {
  if (!m_is_initialized || m_file_system == nullptr) {
    return;
  }
  removeCookStamp(*m_file_system);
}

bool AssetCompilerService::refreshCookStamp() {
  if (!m_is_initialized || m_file_system == nullptr) {
    return false;
  }
  const CookStampFingerprint current =
      computeCookStampFingerprint(*m_file_system);
  return writeCookStamp(*m_file_system, current);
}

void AssetCompilerService::markFinalStale(const eastl::string& guid) {
  if (!m_is_initialized || guid.empty()) {
    return;
  }

  removeIfExists(*m_file_system, cookedMeshPath(*m_file_system, guid));
  removeIfExists(*m_file_system, cookedMeshMetaPath(*m_file_system, guid));
  removeIfExists(*m_file_system, cookedMeshMaterialPath(*m_file_system, guid));
  removeIfExists(*m_file_system, cookedTexturePath(*m_file_system, guid));
  removeIfExists(*m_file_system, cookedTextureMetaPath(*m_file_system, guid));
  invalidateCookStamp();
}

void AssetCompilerService::invalidateAssetAndDependents(
    const eastl::string& guid) {
  if (!m_is_initialized || guid.empty()) {
    return;
  }

  eastl::hash_set<eastl::string> visited;
  eastl::vector<eastl::string> queue;
  queue.push_back(guid);

  while (!queue.empty()) {
    const eastl::string current = queue.back();
    queue.pop_back();
    if (current.empty() || !visited.insert(current).second) {
      continue;
    }

    markFinalStale(current);

    const eastl::vector<eastl::string> dependents =
        m_dependency_graph.dependentsOf(current);
    for (const eastl::string& dependent : dependents) {
      queue.push_back(dependent);
    }
  }
}

bool AssetCompilerService::cookAsset(const eastl::string& guid, bool force) {
  if (!m_is_initialized || guid.empty()) {
    return false;
  }

  const eastl::string descriptor_path = m_asset_registry->resolveGuid(guid);
  if (descriptor_path.empty()) {
    LOG_WARN("[AssetCompiler] cookAsset: unknown guid {}", guid.c_str());
    return false;
  }

  if (endsWith(descriptor_path, ".mesh.yaml")) {
    return cookMeshDescriptor(descriptor_path, force);
  }
  if (endsWith(descriptor_path, ".texture.yaml")) {
    return cookTextureDescriptor(descriptor_path, force);
  }

  LOG_WARN("[AssetCompiler] cookAsset: unsupported descriptor {}",
           descriptor_path.c_str());
  return false;
}

void AssetCompilerService::cookDependents(const eastl::string& guid) {
  if (!m_is_initialized || guid.empty()) {
    return;
  }

  const eastl::vector<eastl::string> dependents =
      m_dependency_graph.dependentsOf(guid);
  for (const eastl::string& dependent : dependents) {
    cookAsset(dependent);
  }
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
                  descriptor_mtime, kMeshCookVersion)) {
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
  const MeshSkinData* skin_ptr =
      mesh->isSkinned() ? &mesh->getSkinData() : nullptr;
  MeshletPayload meshlets{};
  const MeshletPayload* meshlets_ptr = nullptr;
  if (skin_ptr == nullptr) {
    meshlets = buildStaticMeshlets(mesh->getVertices(), mesh->getIndices());
    if (!meshlets.empty()) {
      meshlets_ptr = &meshlets;
    }
  }
  if (!writeMeshCookFile(cooked_path, mesh->getVertices(), mesh->getIndices(),
                         skin_ptr, meshlets_ptr)) {
    return false;
  }

  CookedAssetMeta meta{};
  meta.source_mtime = source_mtime;
  meta.descriptor_mtime = descriptor_mtime;
  meta.cook_format = kMeshCookVersion;
  writeCookMetaFile(meta_path, meta);

  m_asset_registry->registerAsset(descriptor.guid, descriptor_virtual_path);
  LOG_INFO("[AssetCompiler] cooked mesh {} -> {}", descriptor_virtual_path.c_str(),
           cooked_path.generic_string());
  (void)m_asset_manager->hydrateMeshGltfMaterial(mesh);
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
