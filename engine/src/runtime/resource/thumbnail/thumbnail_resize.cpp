#include "runtime/resource/thumbnail/thumbnail_resize.h"

#include "runtime/resource/asset/texture2d_asset.h"

namespace Blunder {

namespace {

float sampleChannel(const uint8_t* pixels, uint32_t width, uint32_t height,
                    float u, float v, uint32_t channel) {
  const float fx = u * static_cast<float>(width - 1);
  const float fy = v * static_cast<float>(height - 1);
  const int x0 = static_cast<int>(fx);
  const int y0 = static_cast<int>(fy);
  const int x1 = x0 < static_cast<int>(width) - 1 ? x0 + 1 : x0;
  const int y1 = y0 < static_cast<int>(height) - 1 ? y0 + 1 : y0;
  const float tx = fx - static_cast<float>(x0);
  const float ty = fy - static_cast<float>(y0);

  const auto at = [&](int x, int y) -> float {
    return static_cast<float>(
        pixels[(static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4u +
               channel]);
  };

  const float c00 = at(x0, y0);
  const float c10 = at(x1, y0);
  const float c01 = at(x0, y1);
  const float c11 = at(x1, y1);
  const float c0 = c00 * (1.0f - tx) + c10 * tx;
  const float c1 = c01 * (1.0f - tx) + c11 * tx;
  return c0 * (1.0f - ty) + c1 * ty;
}

}  // namespace

void resizeRgba8(const uint8_t* src_pixels, uint32_t src_width,
                 uint32_t src_height, uint32_t out_width, uint32_t out_height,
                 eastl::vector<uint8_t>& out_rgba) {
  if (src_pixels == nullptr || src_width == 0 || src_height == 0 ||
      out_width == 0 || out_height == 0) {
    out_rgba.clear();
    return;
  }

  out_rgba.resize(static_cast<size_t>(out_width) * out_height * 4u);
  for (uint32_t y = 0; y < out_height; ++y) {
    const float v =
        out_height > 1 ? static_cast<float>(y) / static_cast<float>(out_height - 1)
                       : 0.0f;
    for (uint32_t x = 0; x < out_width; ++x) {
      const float u =
          out_width > 1 ? static_cast<float>(x) / static_cast<float>(out_width - 1)
                        : 0.0f;
      const size_t dst =
          (static_cast<size_t>(y) * out_width + x) * 4u;
      out_rgba[dst + 0] =
          static_cast<uint8_t>(sampleChannel(src_pixels, src_width, src_height,
                                             u, v, 0));
      out_rgba[dst + 1] =
          static_cast<uint8_t>(sampleChannel(src_pixels, src_width, src_height,
                                             u, v, 1));
      out_rgba[dst + 2] =
          static_cast<uint8_t>(sampleChannel(src_pixels, src_width, src_height,
                                             u, v, 2));
      out_rgba[dst + 3] =
          static_cast<uint8_t>(sampleChannel(src_pixels, src_width, src_height,
                                             u, v, 3));
    }
  }
}

void resizeTexture2DAssetToRgba8(const Texture2DAsset& texture,
                                 uint32_t out_width, uint32_t out_height,
                                 eastl::vector<uint8_t>& out_rgba) {
  resizeRgba8(texture.getPixelData(), texture.getWidth(), texture.getHeight(),
              out_width, out_height, out_rgba);
}

}  // namespace Blunder
