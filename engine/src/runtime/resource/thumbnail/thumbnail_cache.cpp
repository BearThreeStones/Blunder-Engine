#include "runtime/resource/thumbnail/thumbnail_cache.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#include <stb_image_write.h>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/content/content_entry.h"
#include "runtime/resource/thumbnail/scene_thumbnail_fingerprint.h"

namespace Blunder {

namespace {

constexpr const char* k_cache_subdir = ".blunder/cache/thumbnails";

struct PngWriteBuffer {
  eastl::vector<uint8_t> bytes;
};

void pngWriteCallback(void* context, void* data, int size) {
  if (context == nullptr || data == nullptr || size <= 0) {
    return;
  }
  auto* buffer = static_cast<PngWriteBuffer*>(context);
  const size_t offset = buffer->bytes.size();
  buffer->bytes.resize(offset + static_cast<size_t>(size));
  std::memcpy(buffer->bytes.data() + offset, data, static_cast<size_t>(size));
}

uint64_t parseMetaSourceMtime(const eastl::string& meta_text) {
  const char* key = "\"source_mtime\"";
  const char* pos = std::strstr(meta_text.c_str(), key);
  if (pos == nullptr) {
    return 0;
  }
  const char* colon = std::strchr(pos + std::strlen(key), ':');
  if (colon == nullptr) {
    return 0;
  }
  return std::strtoull(colon + 1, nullptr, 10);
}

uint64_t hashFingerprint(const eastl::string& fingerprint) {
  uint64_t hash = 14695981039346656037ull;
  for (size_t i = 0; i < fingerprint.size(); ++i) {
    hash ^= static_cast<uint8_t>(fingerprint[i]);
    hash *= 1099511628211ull;
  }
  return hash;
}

bool endsWithLiteral(const eastl::string& value, const char* suffix) {
  const size_t suffix_len = std::strlen(suffix);
  return value.size() >= suffix_len &&
         value.compare(value.size() - suffix_len, suffix_len, suffix) == 0;
}

}  // namespace

void ThumbnailCache::bind(FileSystem* file_system) {
  m_file_system = file_system;
}

void ThumbnailCache::setAssetRegistry(AssetRegistry* asset_registry) {
  m_asset_registry = asset_registry;
}

eastl::string ThumbnailCache::cacheRoot() const {
  if (m_file_system == nullptr) {
    return {};
  }
  return eastl::string(
      (m_file_system->getProjectRoot() / k_cache_subdir).generic_string().c_str());
}

eastl::string ThumbnailCache::sanitizeVirtualPath(
    const eastl::string& virtual_path) const {
  eastl::string out;
  out.reserve(virtual_path.size());
  for (size_t i = 0; i < virtual_path.size(); ++i) {
    const char c = virtual_path[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') {
      out.push_back(c);
    } else {
      out.push_back('_');
    }
  }
  return out;
}

ThumbnailCachePaths ThumbnailCache::pathsForEntry(
    const ContentEntry& entry) const {
  ThumbnailCachePaths paths{};
  char suffix[32];
  std::snprintf(suffix, sizeof(suffix), "__%llx",
                static_cast<unsigned long long>(entry.modified_time));

  eastl::string stem = sanitizeVirtualPath(entry.virtual_path);
  const bool mesh_like =
      endsWithLiteral(entry.virtual_path, ".mesh.yaml") ||
      endsWithLiteral(entry.virtual_path, ".gltf") ||
      endsWithLiteral(entry.virtual_path, ".glb") ||
      endsWithLiteral(entry.virtual_path, ".mesh.asset");
  if (mesh_like) {
    stem.append("_mpv5");
  }
  if (endsWithLiteral(entry.virtual_path, ".scene.asset") &&
      m_file_system != nullptr) {
    stem.append("_scv1_");
    const eastl::string fingerprint = computeSceneThumbnailFingerprint(
        *m_file_system, m_asset_registry, entry.virtual_path,
        entry.modified_time);
    char fp_hex[32];
    std::snprintf(fp_hex, sizeof(fp_hex), "%llx",
                  static_cast<unsigned long long>(hashFingerprint(fingerprint)));
    stem.append(fp_hex);
  }
  stem.append(suffix);

  const eastl::string root = cacheRoot();
  paths.png_path = root;
  if (!paths.png_path.empty() && paths.png_path.back() != '/' &&
      paths.png_path.back() != '\\') {
    paths.png_path.push_back('/');
  }
  paths.png_path.append(stem);
  paths.png_path.append(".png");

  paths.meta_path = paths.png_path;
  const size_t dot = paths.meta_path.rfind('.');
  if (dot != eastl::string::npos) {
    paths.meta_path.resize(dot);
  }
  paths.meta_path.append(".meta");
  return paths;
}

bool ThumbnailCache::isCacheValid(const ContentEntry& entry,
                                  const ThumbnailCachePaths& paths) const {
  if (!m_file_system || entry.is_directory) {
    return false;
  }
  if (!m_file_system->exists(
          std::filesystem::path(paths.png_path.c_str())) ||
      !m_file_system->exists(
          std::filesystem::path(paths.meta_path.c_str()))) {
    return false;
  }

  eastl::string meta_text;
  if (!m_file_system->readText(std::filesystem::path(paths.meta_path.c_str()),
                               meta_text)) {
    return false;
  }

  const uint64_t recorded_mtime = parseMetaSourceMtime(meta_text);
  return recorded_mtime == entry.modified_time;
}

bool ThumbnailCache::writePng(const ThumbnailCachePaths& paths, uint32_t width,
                              uint32_t height, const uint8_t* rgba_pixels,
                              uint64_t source_mtime) {
  if (!m_file_system || rgba_pixels == nullptr || width == 0 || height == 0) {
    return false;
  }

  PngWriteBuffer png_buffer;
  const int stride = static_cast<int>(width) * 4;
  const int png_ok = stbi_write_png_to_func(
      pngWriteCallback, &png_buffer, static_cast<int>(width),
      static_cast<int>(height), 4, rgba_pixels, stride);
  if (png_ok == 0 || png_buffer.bytes.empty()) {
    LOG_ERROR("[ThumbnailCache] stbi_write_png_to_func failed: {}",
              paths.png_path.c_str());
    return false;
  }

  if (!m_file_system->writeBinary(std::filesystem::path(paths.png_path.c_str()),
                                  png_buffer.bytes.data(),
                                  png_buffer.bytes.size())) {
    LOG_ERROR("[ThumbnailCache] writeBinary failed: {}", paths.png_path.c_str());
    return false;
  }

  char meta[192];
  std::snprintf(meta, sizeof(meta),
                "{\n  \"source_mtime\": %llu,\n  \"width\": %u,\n  "
                "\"height\": %u,\n  \"generator\": \"thumbnail-cache\"\n}\n",
                static_cast<unsigned long long>(source_mtime),
                static_cast<unsigned>(width), static_cast<unsigned>(height));

  if (!m_file_system->writeText(std::filesystem::path(paths.meta_path.c_str()),
                                eastl::string(meta))) {
    LOG_ERROR("[ThumbnailCache] failed to write meta: {}",
              paths.meta_path.c_str());
    return false;
  }

  return true;
}

}  // namespace Blunder
