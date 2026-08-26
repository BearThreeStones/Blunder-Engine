#include "runtime/function/render/scene_thumbnail/scene_still.h"

#include <algorithm>

namespace Blunder {

bool fitRgbaToSceneStill(const uint8_t* src, uint32_t src_w, uint32_t src_h,
                         uint32_t aspect_w, uint32_t aspect_h,
                         uint32_t longest_edge, eastl::vector<uint8_t>& out_rgba,
                         SceneStillExtent& out_extent) {
  out_rgba.clear();
  out_extent = {};
  if (src == nullptr || src_w == 0 || src_h == 0) {
    return false;
  }
  out_extent = sceneStillExtent(aspect_w, aspect_h, longest_edge);
  if (out_extent.width == 0 || out_extent.height == 0) {
    return false;
  }

  const double src_aspect =
      static_cast<double>(src_w) / static_cast<double>(src_h);
  const double dst_aspect = static_cast<double>(out_extent.width) /
                            static_cast<double>(out_extent.height);

  uint32_t crop_w = src_w;
  uint32_t crop_h = src_h;
  uint32_t crop_x = 0;
  uint32_t crop_y = 0;
  if (src_aspect > dst_aspect) {
    crop_w = static_cast<uint32_t>(
        std::max(1.0, static_cast<double>(src_h) * dst_aspect + 0.5));
    if (crop_w > src_w) {
      crop_w = src_w;
    }
    crop_x = (src_w - crop_w) / 2u;
  } else if (src_aspect < dst_aspect) {
    crop_h = static_cast<uint32_t>(
        std::max(1.0, static_cast<double>(src_w) / dst_aspect + 0.5));
    if (crop_h > src_h) {
      crop_h = src_h;
    }
    crop_y = (src_h - crop_h) / 2u;
  }

  out_rgba.resize(static_cast<size_t>(out_extent.width) * out_extent.height *
                  4u);
  for (uint32_t y = 0; y < out_extent.height; ++y) {
    const uint32_t src_y =
        crop_y +
        std::min(crop_h - 1u, (y * crop_h) / out_extent.height);
    for (uint32_t x = 0; x < out_extent.width; ++x) {
      const uint32_t src_x =
          crop_x +
          std::min(crop_w - 1u, (x * crop_w) / out_extent.width);
      const size_t si =
          (static_cast<size_t>(src_y) * src_w + src_x) * 4u;
      const size_t di =
          (static_cast<size_t>(y) * out_extent.width + x) * 4u;
      out_rgba[di] = src[si];
      out_rgba[di + 1] = src[si + 1];
      out_rgba[di + 2] = src[si + 2];
      out_rgba[di + 3] = src[si + 3];
    }
  }
  return true;
}

}  // namespace Blunder
