#include "runtime/resource/asset_cook/texture_cooker.h"

#include <cstring>
#include <fstream>

#include "runtime/core/base/macro.h"

namespace Blunder {

bool writeTextureCookFile(const std::filesystem::path& output_path,
                          uint32_t width, uint32_t height,
                          const uint8_t* rgba_pixels, bool srgb) {
  if (width == 0 || height == 0 || rgba_pixels == nullptr) {
    return false;
  }

  TextureCookHeader header{};
  std::memcpy(header.magic, kTextureCookMagic, sizeof(header.magic));
  header.version = kTextureCookVersion;
  header.width = width;
  header.height = height;
  header.srgb = srgb ? 1u : 0u;

  std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }

  stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
  const size_t pixel_bytes = static_cast<size_t>(width) * height * 4u;
  stream.write(reinterpret_cast<const char*>(rgba_pixels),
               static_cast<std::streamsize>(pixel_bytes));
  return stream.good();
}

bool readTextureCookFile(const std::filesystem::path& input_path,
                         uint32_t& out_width, uint32_t& out_height,
                         eastl::vector<uint8_t>& out_pixels) {
  std::ifstream stream(input_path, std::ios::binary);
  if (!stream) {
    return false;
  }

  TextureCookHeader header{};
  stream.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!stream) {
    return false;
  }
  if (std::memcmp(header.magic, kTextureCookMagic, sizeof(header.magic)) != 0 ||
      header.version != kTextureCookVersion || header.width == 0 ||
      header.height == 0) {
    return false;
  }

  out_width = header.width;
  out_height = header.height;
  const size_t pixel_bytes =
      static_cast<size_t>(header.width) * header.height * 4u;
  out_pixels.resize(pixel_bytes);
  stream.read(reinterpret_cast<char*>(out_pixels.data()),
              static_cast<std::streamsize>(pixel_bytes));
  return stream.good();
}

}  // namespace Blunder
