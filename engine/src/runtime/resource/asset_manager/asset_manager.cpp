#include "runtime/resource/asset_manager/asset_manager.h"

#include <stb_image.h>

#include <cstring>
#include <filesystem>

#include "EASTL/utility.h"
#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"

namespace Blunder {

namespace {
uint64_t querySourceTimestamp(const std::filesystem::path& path) {
  std::error_code ec;
  const auto t = std::filesystem::last_write_time(path, ec);
  if (ec) {
    return 0;
  }
  return static_cast<uint64_t>(t.time_since_epoch().count());
}
}  // namespace

void AssetManager::initialize(const AssetManagerInitInfo& info) {
  if (m_is_initialized) {
    return;
  }

  if (info.file_system == nullptr) {
    LOG_FATAL("[AssetManager] initialize() requires a non-null FileSystem");
  }

  m_file_system = info.file_system;
  m_is_initialized = true;
  LOG_INFO("[AssetManager] initialized (asset root: {})",
           m_file_system->getAssetRoot().generic_string());
}

void AssetManager::shutdown() {
  if (!m_is_initialized) {
    return;
  }
  clearCache();
  m_file_system = nullptr;
  m_is_initialized = false;
}

eastl::shared_ptr<Texture2DAsset> AssetManager::loadTexture2D(
    const eastl::string& virtual_path) {
  if (!m_is_initialized) {
    LOG_ERROR("[AssetManager] loadTexture2D before initialize()");
    return nullptr;
  }
  if (virtual_path.empty()) {
    LOG_ERROR("[AssetManager] loadTexture2D: empty path");
    return nullptr;
  }

  const eastl::string key = canonicalKey(virtual_path);
  if (auto it = m_texture_cache.find(key);
      it != m_texture_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_texture_cache.erase(it);
  }

  const auto absolute =
      m_file_system->resolveAsset(std::filesystem::path(key.c_str()));

  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(absolute, bytes)) {
    LOG_ERROR("[AssetManager] loadTexture2D: cannot read {}",
              absolute.generic_string());
    return nullptr;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  // Always decode to 4 channels (RGBA8) so the GPU upload path stays uniform.
  stbi_uc* decoded = stbi_load_from_memory(
      bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels,
      STBI_rgb_alpha);

  if (decoded == nullptr) {
    LOG_ERROR("[AssetManager] loadTexture2D: stb_image failed for {} ({})",
              absolute.generic_string(),
              stbi_failure_reason() ? stbi_failure_reason() : "unknown");
    return nullptr;
  }

  const size_t pixel_byte_size =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
  eastl::vector<uint8_t> pixels(pixel_byte_size);
  std::memcpy(pixels.data(), decoded, pixel_byte_size);
  stbi_image_free(decoded);

  Asset::Meta meta;
  meta.virtual_path = key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset = eastl::make_shared<Texture2DAsset>(
      eastl::move(meta), static_cast<uint32_t>(width),
      static_cast<uint32_t>(height), 4u, eastl::move(pixels));

  m_texture_cache[key] = asset;

  LOG_INFO("[AssetManager] loaded Texture2D {} (resolved: {}, {}x{}, src_channels={})",
           key.c_str(), absolute.generic_string(), width, height, channels);
  return asset;
}

eastl::shared_ptr<ShaderAsset> AssetManager::loadShader(
    const eastl::string& virtual_path) {
  if (!m_is_initialized) {
    LOG_ERROR("[AssetManager] loadShader before initialize()");
    return nullptr;
  }
  if (virtual_path.empty()) {
    LOG_ERROR("[AssetManager] loadShader: empty path");
    return nullptr;
  }

  const eastl::string key = canonicalKey(virtual_path);
  if (auto it = m_shader_cache.find(key); it != m_shader_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_shader_cache.erase(it);
  }

  const auto absolute =
      m_file_system->resolveShader(std::filesystem::path(key.c_str()));
  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(absolute, bytes)) {
    LOG_ERROR("[AssetManager] loadShader: cannot read {}",
              absolute.generic_string());
    return nullptr;
  }

  ShaderAsset::Stage stage = ShaderAsset::Stage::Unknown;
  const eastl::string ext = eastl::string(absolute.extension().generic_string().c_str());
  if (ext == ".vert") {
    stage = ShaderAsset::Stage::Vertex;
  } else if (ext == ".frag") {
    stage = ShaderAsset::Stage::Fragment;
  } else if (ext == ".comp") {
    stage = ShaderAsset::Stage::Compute;
  }

  Asset::Meta meta;
  meta.virtual_path = key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset =
      eastl::make_shared<ShaderAsset>(eastl::move(meta), stage, eastl::move(bytes));
  m_shader_cache[key] = asset;
  LOG_INFO("[AssetManager] loaded Shader {} (resolved: {}, bytes={})",
           key.c_str(), absolute.generic_string(), asset->getByteSize());
  return asset;
}

eastl::shared_ptr<MeshAsset> AssetManager::loadMesh(
    const eastl::string& virtual_path) {
  if (!m_is_initialized) {
    LOG_ERROR("[AssetManager] loadMesh before initialize()");
    return nullptr;
  }
  if (virtual_path.empty()) {
    LOG_ERROR("[AssetManager] loadMesh: empty path");
    return nullptr;
  }

  const eastl::string key = canonicalKey(virtual_path);
  if (auto it = m_mesh_cache.find(key); it != m_mesh_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_mesh_cache.erase(it);
  }

  const auto absolute =
      m_file_system->resolveAsset(std::filesystem::path(key.c_str()));
  eastl::vector<uint8_t> bytes;
  if (!m_file_system->readBinary(absolute, bytes)) {
    LOG_ERROR("[AssetManager] loadMesh: cannot read {}", absolute.generic_string());
    return nullptr;
  }

  Asset::Meta meta;
  meta.virtual_path = key;
  meta.absolute_path = absolute;
  meta.source_timestamp = querySourceTimestamp(absolute);
  auto asset = eastl::make_shared<MeshAsset>(eastl::move(meta), eastl::move(bytes), 0u,
                                             eastl::vector<uint32_t>{});
  m_mesh_cache[key] = asset;
  LOG_INFO(
      "[AssetManager] loaded Mesh {} (resolved: {}, vertex_blob_bytes={}, parser=pending)",
      key.c_str(), absolute.generic_string(), asset->getVertexByteSize());
  return asset;
}

AssetHandle AssetManager::makeHandle(Asset::Type type,
                                     const eastl::string& virtual_path) const {
  AssetHandle handle;
  handle.type = type;
  handle.key = canonicalKey(virtual_path);
  handle.generation = 0;
  return handle;
}

eastl::shared_ptr<Asset> AssetManager::get(const AssetHandle& handle) const {
  if (!handle.isValid()) {
    return nullptr;
  }

  switch (handle.type) {
    case Asset::Type::Texture2D: {
      auto it = m_texture_cache.find(handle.key);
      if (it == m_texture_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    case Asset::Type::Shader: {
      auto it = m_shader_cache.find(handle.key);
      if (it == m_shader_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    case Asset::Type::Mesh: {
      auto it = m_mesh_cache.find(handle.key);
      if (it == m_mesh_cache.end()) {
        return nullptr;
      }
      auto ptr = it->second.lock();
      if (!ptr) {
        return nullptr;
      }
      return eastl::static_pointer_cast<Asset>(ptr);
    }
    default:
      return nullptr;
  }
}

void AssetManager::garbageCollect() {
  for (auto it = m_texture_cache.begin(); it != m_texture_cache.end();) {
    if (it->second.expired()) {
      it = m_texture_cache.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_shader_cache.begin(); it != m_shader_cache.end();) {
    if (it->second.expired()) {
      it = m_shader_cache.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = m_mesh_cache.begin(); it != m_mesh_cache.end();) {
    if (it->second.expired()) {
      it = m_mesh_cache.erase(it);
    } else {
      ++it;
    }
  }
}

void AssetManager::clearCache() {
  m_texture_cache.clear();
  m_shader_cache.clear();
  m_mesh_cache.clear();
}

eastl::string AssetManager::canonicalKey(const eastl::string& virtual_path) const {
  const std::filesystem::path normalized =
      std::filesystem::path(virtual_path.c_str()).lexically_normal();
  return eastl::string(normalized.generic_string().c_str());
}

}  // namespace Blunder
