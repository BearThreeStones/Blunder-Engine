#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/thumbnail/thumbnail_types.h"

namespace Blunder {

/// One file or directory discovered under a content root (for Asset Browser).
struct ContentEntry {
  eastl::string virtual_path;
  ContentRoot root{ContentRoot::Assets};
  bool is_directory{false};
  uint64_t size_bytes{0};
  uint64_t modified_time{0};
  ThumbnailStatus thumbnail_status{ThumbnailStatus::None};
  eastl::string thumbnail_cache_path;
};

}  // namespace Blunder
