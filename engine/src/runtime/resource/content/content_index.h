#pragma once

#include "EASTL/vector.h"

namespace Blunder {

class AssetManager;
class FileSystem;
class ThumbnailGenerator;
struct ContentEntry;

/// Flat index of project content for editor browsers (filesystem scan only).
class ContentIndex final {
 public:
  /// Scan Assets and Resources trees. Skips `.gitkeep` files.
  static eastl::vector<ContentEntry> scan(const FileSystem& file_system,
                                          int32_t max_depth = -1);
};

eastl::vector<ContentEntry> buildContentIndexWithThumbnails(
    FileSystem& file_system, AssetManager& asset_manager,
    ThumbnailGenerator& thumbnail_generator, int32_t max_depth = -1);

}  // namespace Blunder
