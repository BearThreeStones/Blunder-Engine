#include "runtime/resource/asset_manager/asset_manager.h"

#include <stb_image.h>

#include <cstring>

#include "EASTL/utility.h"
#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"

namespace Blunder {

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

  if (auto it = m_texture_cache.find(virtual_path);
      it != m_texture_cache.end()) {
    if (auto cached = it->second.lock()) {
      return cached;
    }
    m_texture_cache.erase(it);
  }

  const auto absolute = m_file_system->resolveAsset(
      std::filesystem::path(virtual_path.c_str()));

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

  auto asset = eastl::make_shared<Texture2DAsset>(
      virtual_path, static_cast<uint32_t>(width),
      static_cast<uint32_t>(height), 4u, eastl::move(pixels));

  m_texture_cache[virtual_path] = asset;

  LOG_INFO("[AssetManager] loaded Texture2D {} ({}x{}, src_channels={})",
           virtual_path.c_str(), width, height, channels);
  return asset;
}

void AssetManager::garbageCollect() {
  for (auto it = m_texture_cache.begin(); it != m_texture_cache.end();) {
    if (it->second.expired()) {
      it = m_texture_cache.erase(it);
    } else {
      ++it;
    }
  }
}

void AssetManager::clearCache() { m_texture_cache.clear(); }

}  // namespace Blunder
