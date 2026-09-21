#pragma once

#include <algorithm>
#include <cstdint>

namespace Blunder {

constexpr float k_vrs_sobel_threshold = 0.1f;
constexpr uint8_t k_vrs_texel_1x1 = 0;
constexpr uint8_t k_vrs_texel_2x2 = static_cast<uint8_t>((1u << 2) | 1u);

/// Rec.709 luminance (Packt ch.9 `luminance()`).
inline float rec709Luminance(float r, float g, float b) {
  return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

/// Vulkan fragment shading rate texel packing: size = 2^((t>>2)&3) × 2^(t&3).
inline uint8_t packFragmentShadingRate(uint32_t log2_width, uint32_t log2_height) {
  return static_cast<uint8_t>(((log2_width & 3u) << 2) | (log2_height & 3u));
}

inline void unpackFragmentShadingRate(uint8_t texel, uint32_t* width,
                                      uint32_t* height) {
  if (width != nullptr) {
    *width = 1u << ((texel >> 2) & 3u);
  }
  if (height != nullptr) {
    *height = 1u << (texel & 3u);
  }
}

/// `G > 0.1` → 1×1 (texel 0); else 2×2 (`1 << 2 | 1`).
inline uint8_t encodeSobelShadingRate(float gradient_sq,
                                      float threshold = k_vrs_sobel_threshold) {
  return gradient_sq > threshold ? k_vrs_texel_1x1 : k_vrs_texel_2x2;
}

/// 3×3 Sobel `G = dx² + dy²` on a luminance neighbourhood (row-major, centre at 4).
inline float sobelGradientSq(const float luma[9]) {
  const float dx = -luma[0] - 2.0f * luma[3] - luma[6] + luma[2] + 2.0f * luma[5] +
                   luma[8];
  const float dy = -luma[0] - 2.0f * luma[1] - luma[2] + luma[6] + 2.0f * luma[7] +
                   luma[8];
  return dx * dx + dy * dy;
}

/// `{1,1}` when inside `[min, max]`; otherwise the device **min**.
inline void chooseFragmentShadingRateTexelSize(uint32_t min_w, uint32_t min_h,
                                               uint32_t max_w, uint32_t max_h,
                                               uint32_t* out_w, uint32_t* out_h) {
  const uint32_t min_width = std::max(1u, min_w);
  const uint32_t min_height = std::max(1u, min_h);
  const uint32_t max_width = std::max(min_width, max_w);
  const uint32_t max_height = std::max(min_height, max_h);
  const bool one_ok = min_width <= 1u && min_height <= 1u && max_width >= 1u &&
                      max_height >= 1u;
  if (out_w != nullptr) {
    *out_w = one_ok ? 1u : min_width;
  }
  if (out_h != nullptr) {
    *out_h = one_ok ? 1u : min_height;
  }
}

inline bool fragmentShadingRateWanted(bool has_extension, bool attachment_rate,
                                      bool pipeline_rate, bool has_renderpass2) {
  return has_extension && attachment_rate && pipeline_rate && has_renderpass2;
}

}  // namespace Blunder
