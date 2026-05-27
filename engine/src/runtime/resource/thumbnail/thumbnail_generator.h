#pragma once

#include <cstdint>

#include "EASTL/vector.h"

#include "runtime/resource/thumbnail/thumbnail_cache.h"
#include "runtime/resource/thumbnail/thumbnail_placeholders.h"
#include "runtime/resource/thumbnail/thumbnail_types.h"

namespace Blunder {

class AssetManager;
class FileSystem;
struct ContentEntry;

struct ThumbnailGeneratorInit {
  FileSystem* file_system{nullptr};
  AssetManager* asset_manager{nullptr};
  uint32_t thumbnail_size{128};
};

class ThumbnailGenerator final {
 public:
  ThumbnailGenerator() = default;

  void initialize(const ThumbnailGeneratorInit& init);
  void shutdown();

  ThumbnailResult ensureThumbnail(const ContentEntry& entry);
  void ensureThumbnails(const ContentEntry* entries, uint32_t entry_count);

 private:
  bool generateRgbaForEntry(const ContentEntry& entry,
                            eastl::vector<uint8_t>& out_rgba);
  bool generateMeshThumbnail(const eastl::string& virtual_path,
                             eastl::vector<uint8_t>& out_rgba);
  bool generateImageThumbnail(const eastl::string& virtual_path,
                              eastl::vector<uint8_t>& out_rgba);
  bool generatePlaceholder(ThumbnailPlaceholderKind kind,
                           eastl::vector<uint8_t>& out_rgba);
  bool resolveDescriptorSource(const eastl::string& descriptor_virtual_path,
                               eastl::string& out_source_path) const;

  static bool shouldSkipEntry(const ContentEntry& entry);
  static bool endsWithSuffix(const eastl::string& value, const char* suffix);
  static eastl::string extensionLower(const eastl::string& virtual_path);

  FileSystem* m_file_system{nullptr};
  AssetManager* m_asset_manager{nullptr};
  ThumbnailCache m_cache;
  uint32_t m_thumbnail_size{128};
  bool m_is_initialized{false};
};

}  // namespace Blunder
