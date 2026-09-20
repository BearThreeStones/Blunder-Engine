#pragma once

#include <cstdint>
#include <functional>

#include "EASTL/vector.h"

namespace Blunder {

/// CPU-readback the Player color target and fit to Capture 16:9 aspect.
/// Not a Scene still and not HWND scrape.
bool capturePlayProcessFrame(eastl::vector<uint8_t>& out_rgba, uint32_t& out_width,
                             uint32_t& out_height);

/// Pump until TextureLoader GPU copies finish, then a few extra ticks so the
/// residency-changed skip-draw does not capture bindless slot 0.
void waitUntilTextureUploadsIdle(uint32_t timeout_ms,
                                 const std::function<void()>& pump);

}  // namespace Blunder
