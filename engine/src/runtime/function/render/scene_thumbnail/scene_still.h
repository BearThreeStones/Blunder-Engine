#pragma once

#include <cstdint>

#include "EASTL/vector.h"

namespace Blunder {

/// Pixel size of a Scene still from aspect ratio + longest-edge cap.
struct SceneStillExtent {
  uint32_t width{0};
  uint32_t height{0};
};

inline constexpr uint32_t k_capture_aspect_w = 16;
inline constexpr uint32_t k_capture_aspect_h = 9;
inline constexpr uint32_t k_capture_longest_edge = 1280;

inline SceneStillExtent sceneStillExtent(uint32_t aspect_w, uint32_t aspect_h,
                                         uint32_t longest_edge) {
  SceneStillExtent out{};
  if (aspect_w == 0 || aspect_h == 0 || longest_edge == 0) {
    return out;
  }
  if (aspect_w >= aspect_h) {
    out.width = longest_edge;
    out.height = (longest_edge * aspect_h) / aspect_w;
    if (out.height == 0) {
      out.height = 1;
    }
  } else {
    out.height = longest_edge;
    out.width = (longest_edge * aspect_w) / aspect_h;
    if (out.width == 0) {
      out.width = 1;
    }
  }
  return out;
}

inline SceneStillExtent captureStillExtent() {
  return sceneStillExtent(k_capture_aspect_w, k_capture_aspect_h,
                          k_capture_longest_edge);
}

/// Center-crop @p src to @p aspect, then nearest-neighbor scale to the capped
/// still size. @p src is tightly packed RGBA8.
bool fitRgbaToSceneStill(const uint8_t* src, uint32_t src_w, uint32_t src_h,
                         uint32_t aspect_w, uint32_t aspect_h,
                         uint32_t longest_edge, eastl::vector<uint8_t>& out_rgba,
                         SceneStillExtent& out_extent);

}  // namespace Blunder
