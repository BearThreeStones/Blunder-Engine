#pragma once

#include <cstdint>

#include "EASTL/vector.h"

namespace Blunder {

/// CPU-readback the Player color target and fit to Capture 16:9 aspect.
/// Not a Scene still and not HWND scrape.
bool capturePlayProcessFrame(eastl::vector<uint8_t>& out_rgba, uint32_t& out_width,
                             uint32_t& out_height);

}  // namespace Blunder
