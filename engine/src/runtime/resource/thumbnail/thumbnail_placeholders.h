#pragma once

#include <cstdint>

#include "EASTL/vector.h"

namespace Blunder {

enum class ThumbnailPlaceholderKind : uint8_t {
  Folder = 0,
  Mesh,
  File,
};

/// Fills `out_rgba` with `width * height * 4` RGBA8 pixels (procedural icon).
void fillThumbnailPlaceholder(ThumbnailPlaceholderKind kind, uint32_t width,
                              uint32_t height, eastl::vector<uint8_t>& out_rgba);

}  // namespace Blunder
