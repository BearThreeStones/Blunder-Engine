#pragma once

#include <cstdint>

namespace Blunder {

/// CPU RGBA8 viewport payload for presentation to the UI sink.
struct ViewportCpuFrame {
  const uint8_t* pixels_rgba{nullptr};
  uint32_t width{0};
  uint32_t height{0};
  uint32_t stride_bytes{0};
};

}  // namespace Blunder
