#include "runtime/resource/asset_import/companion_animation_gltf.h"

#include <fstream>
#include <vector>

#include <cgltf.h>

namespace Blunder {

namespace {

bool isCompanionAnimationGltfDocument(const cgltf_data* data) {
  if (data == nullptr) {
    return false;
  }
  return data->animations_count > 0 && data->meshes_count == 0;
}

bool readFileBytes(const std::filesystem::path& path,
                   std::vector<std::uint8_t>& out_bytes) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return false;
  }
  input.seekg(0, std::ios::end);
  const std::streamoff size = input.tellg();
  if (size <= 0) {
    return false;
  }
  out_bytes.resize(static_cast<size_t>(size));
  input.seekg(0, std::ios::beg);
  input.read(reinterpret_cast<char*>(out_bytes.data()), size);
  return static_cast<bool>(input);
}

}  // namespace

bool isCompanionAnimationGltf(const std::filesystem::path& gltf_absolute) {
  std::vector<std::uint8_t> bytes;
  if (!readFileBytes(gltf_absolute, bytes) || bytes.empty()) {
    return false;
  }

  cgltf_options options{};
  cgltf_data* data = nullptr;
  const cgltf_result parse_result =
      cgltf_parse(&options, bytes.data(), bytes.size(), &data);
  if (parse_result != cgltf_result_success || data == nullptr) {
    return false;
  }

  const bool accepted = isCompanionAnimationGltfDocument(data);
  cgltf_free(data);
  return accepted;
}

}  // namespace Blunder
