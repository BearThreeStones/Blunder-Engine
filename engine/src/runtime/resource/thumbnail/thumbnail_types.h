#pragma once

#include <cstdint>

#include "EASTL/string.h"

namespace Blunder {

enum class ThumbnailStatus : uint8_t {
  None = 0,
  Skipped,
  CacheHit,
  Generated,
  Failed,
};

struct ThumbnailResult {
  ThumbnailStatus status{ThumbnailStatus::None};
  eastl::string cache_path;
};

}  // namespace Blunder
