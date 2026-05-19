#pragma once

#include <cstdint>

#include "EASTL/vector.h"

namespace Blunder {

class Texture2DAsset;

/// Bilinear downscale of RGBA8 source into `out_rgba` (`out_width` x `out_height`).
void resizeRgba8(const uint8_t* src_pixels, uint32_t src_width,
                 uint32_t src_height, uint32_t out_width, uint32_t out_height,
                 eastl::vector<uint8_t>& out_rgba);

void resizeTexture2DAssetToRgba8(const Texture2DAsset& texture,
                                 uint32_t out_width, uint32_t out_height,
                                 eastl::vector<uint8_t>& out_rgba);

}  // namespace Blunder
