#pragma once

#include <algorithm>
#include <cstdint>

namespace Blunder {

inline constexpr uint32_t kCameraPreviewMaxLongEdgePx = 480;

struct CameraPreviewRtSize {
  uint32_t width{0};
  uint32_t height{0};
  bool ok{false};
};

/// Map logical content size to GPU RT size; preserve aspect; clamp longest edge.
inline CameraPreviewRtSize computeCameraPreviewRtSize(float content_w,
                                                      float content_h) {
  CameraPreviewRtSize out{};
  if (!(content_w > 0.5f) || !(content_h > 0.5f)) {
    return out;
  }
  float w = content_w;
  float h = content_h;
  const float long_edge = std::max(w, h);
  if (long_edge > static_cast<float>(kCameraPreviewMaxLongEdgePx)) {
    const float s = static_cast<float>(kCameraPreviewMaxLongEdgePx) / long_edge;
    w *= s;
    h *= s;
  }
  out.width = std::max(1u, static_cast<uint32_t>(w + 0.5f));
  out.height = std::max(1u, static_cast<uint32_t>(h + 0.5f));
  out.ok = true;
  return out;
}

}  // namespace Blunder
