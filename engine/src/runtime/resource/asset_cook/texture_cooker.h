#pragma once

#include <cstdint>

#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

inline constexpr char kTextureCookMagic[4] = {'B', 'L', 'T', 'X'};
inline constexpr uint32_t kTextureCookVersion = 1;

#pragma pack(push, 1)
struct TextureCookHeader {
  char magic[4];
  uint32_t version{0};
  uint32_t width{0};
  uint32_t height{0};
  uint32_t srgb{0};
};
#pragma pack(pop)

/// Writes a cooked RGBA8 texture blob.
bool writeTextureCookFile(const std::filesystem::path& output_path,
                          uint32_t width, uint32_t height,
                          const uint8_t* rgba_pixels, bool srgb);

bool readTextureCookFile(const std::filesystem::path& input_path,
                         uint32_t& out_width, uint32_t& out_height,
                         eastl::vector<uint8_t>& out_pixels);

}  // namespace Blunder
