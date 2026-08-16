#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"
#include "runtime/platform/file_system/file_system.h"
#include "runtime/resource/content/content_entry.h"

namespace Blunder {

enum class BrowserEntryKind : int32_t {
  folder = 0,
  mesh = 1,
  scene = 2,
  texture = 3,
  animation_clip = 4,
  file = 5,
};

enum class BrowserGridSortColumn : int32_t {
  name = 0,
  type = 1,
  size = 2,
  date = 3,
};

struct ContentBrowserTreeRow {
  eastl::string virtual_path;
  eastl::string display_name;
  int32_t depth{0};
  bool is_directory{false};
  bool is_expanded{false};
  bool has_children{false};
};

struct ContentBrowserGridItem {
  eastl::string virtual_path;
  eastl::string display_name;
  eastl::string thumbnail_cache_path;
  ThumbnailStatus thumbnail_status{ThumbnailStatus::None};
  bool is_directory{false};
  BrowserEntryKind type_kind{BrowserEntryKind::file};
  eastl::string type_label;
  eastl::string size_text;
  eastl::string date_text;
  uint64_t size_bytes{0};
  uint64_t modified_time{0};
};

struct ContentBrowserPathSegment {
  eastl::string virtual_path;
  eastl::string display_name;
};

struct ContentBrowserRefreshStats {
  uint32_t entry_count{0};
  uint32_t thumbnails_generated{0};
  uint32_t thumbnails_cached{0};
  uint32_t thumbnails_skipped{0};
  uint32_t thumbnails_failed{0};
};

}  // namespace Blunder
