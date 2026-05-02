#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset.h"

namespace Blunder {

/// A 2D texture loaded from disk. Pixel storage layout is row-major,
/// origin = top-left, RGBA8 tightly packed (channels = 4).
///
/// Higher-level systems (RenderSystem, materials) consume this CPU-side
/// representation to build their own GPU resources (VkImage, sampler, etc.).
class Texture2DAsset final : public Asset {
 public:
  Texture2DAsset(eastl::string name, uint32_t width, uint32_t height,
                 uint32_t channels, eastl::vector<uint8_t> pixels)
      : Asset(Asset::Type::Texture2D, eastl::move(name)),
        m_width(width),
        m_height(height),
        m_channels(channels),
        m_pixels(eastl::move(pixels)) {}

  uint32_t getWidth() const { return m_width; }
  uint32_t getHeight() const { return m_height; }
  uint32_t getChannels() const { return m_channels; }

  const eastl::vector<uint8_t>& getPixels() const { return m_pixels; }
  const uint8_t* getPixelData() const { return m_pixels.data(); }
  size_t getPixelByteSize() const { return m_pixels.size(); }

 private:
  uint32_t m_width{0};
  uint32_t m_height{0};
  uint32_t m_channels{0};  // always 4 (RGBA8) at the moment
  eastl::vector<uint8_t> m_pixels;
};

}  // namespace Blunder
