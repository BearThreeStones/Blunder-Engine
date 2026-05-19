#pragma once

#include <cstdint>

#include "EASTL/string.h"

namespace Blunder {

class FileSystem;
struct ContentEntry;

struct ThumbnailCachePaths {
  eastl::string png_path;
  eastl::string meta_path;
};

/// Disk cache under `<project>/.blunder/cache/thumbnails/`.
class ThumbnailCache final {
 public:
  void bind(FileSystem* file_system);

  eastl::string cacheRoot() const;
  ThumbnailCachePaths pathsForEntry(const ContentEntry& entry) const;

  bool isCacheValid(const ContentEntry& entry,
                    const ThumbnailCachePaths& paths) const;

  bool writePng(const ThumbnailCachePaths& paths, uint32_t width, uint32_t height,
                const uint8_t* rgba_pixels, uint64_t source_mtime);

 private:
  eastl::string sanitizeVirtualPath(const eastl::string& virtual_path) const;

  FileSystem* m_file_system{nullptr};
};

}  // namespace Blunder
