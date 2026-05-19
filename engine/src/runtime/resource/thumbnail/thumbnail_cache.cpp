#include "runtime/resource/thumbnail/thumbnail_cache.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#include <stb_image_write.h>

#include "runtime/core/base/macro.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/content/content_entry.h"

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

}  // namespace

void ThumbnailCache::bind(FileSystem* file_system) {
  m_file_system = file_system;
}

eastl::string ThumbnailCache::cacheRoot() const {
  ASSERT(m_file_system);
  return eastl::string(
      m_file_system->resolve(k_cache_subdir).generic_string().c_str());
}

eastl::string ThumbnailCache::sanitizeVirtualPath(
    const eastl::string& virtual_path) const {
  eastl::string sanitized;
  sanitized.reserve(virtual_path.size());
  for (char c : virtual_path) {
    if (c == '/' || c == '\\' || c == ':') {
      sanitized.push_back('_');
    } else {
      sanitized.push_back(c);
    }
  }
  return sanitized;
}

ThumbnailCachePaths ThumbnailCache::pathsForEntry(
    const ContentEntry& entry) const {
  ThumbnailCachePaths paths{};
  char suffix[32];
  std::snprintf(suffix, sizeof(suffix), "__%llx",
                static_cast<unsigned long long>(entry.modified_time));

  eastl::string stem = sanitizeVirtualPath(entry.virtual_path);
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

  char meta[128];
  std::snprintf(meta, sizeof(meta),
                "{\n  \"source_mtime\": %llu,\n  \"width\": %u,\n  "
                "\"height\": %u\n}\n",
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
