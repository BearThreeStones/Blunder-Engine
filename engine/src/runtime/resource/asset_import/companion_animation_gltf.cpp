#include "runtime/resource/asset_import/companion_animation_gltf.h"

#include <fstream>
#include <string>
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

bool isGltfOrGlbExtension(const std::filesystem::path& path) {
  const std::string ext = path.extension().string();
  return ext == ".gltf" || ext == ".glb" || ext == ".GLTF" || ext == ".GLB";
}

std::filesystem::path normalizeAbsolutePath(const std::filesystem::path& path) {
  std::error_code ec;
  const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
  if (ec) {
    return path.lexically_normal();
  }
  const std::filesystem::path canonical =
      std::filesystem::weakly_canonical(absolute, ec);
  return ec ? absolute.lexically_normal() : canonical;
}

void collectGltfGlbsInDirectory(const std::filesystem::path& directory,
                                  const std::filesystem::path& exclude_absolute,
                                  std::vector<std::filesystem::path>& out) {
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec) || ec) {
    return;
  }

  const std::filesystem::path exclude_normalized =
      normalizeAbsolutePath(exclude_absolute);

  for (const auto& entry :
       std::filesystem::directory_iterator(directory, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file(ec) || ec) {
      continue;
    }
    const std::filesystem::path file_path = entry.path();
    if (!isGltfOrGlbExtension(file_path)) {
      continue;
    }
    const std::filesystem::path absolute = normalizeAbsolutePath(file_path);
    if (absolute == exclude_normalized) {
      continue;
    }
    out.push_back(absolute);
  }
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

std::vector<std::filesystem::path> enumerateNearDiskCompanionGltfCandidates(
    const std::filesystem::path& mesh_gltf_absolute) {
  std::vector<std::filesystem::path> candidates;
  const std::filesystem::path mesh_absolute =
      normalizeAbsolutePath(mesh_gltf_absolute);
  const std::filesystem::path mesh_dir = mesh_absolute.parent_path();
  if (mesh_dir.empty()) {
    return candidates;
  }

  collectGltfGlbsInDirectory(mesh_dir, mesh_absolute, candidates);

  const std::filesystem::path parent_dir = mesh_dir.parent_path();
  if (parent_dir.empty() || parent_dir == mesh_dir) {
    return candidates;
  }

  std::error_code ec;
  if (!std::filesystem::is_directory(parent_dir, ec) || ec) {
    return candidates;
  }

  const std::filesystem::path mesh_dir_normalized =
      normalizeAbsolutePath(mesh_dir);

  for (const auto& entry :
       std::filesystem::directory_iterator(parent_dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_directory(ec) || ec) {
      continue;
    }
    const std::filesystem::path child_dir =
        normalizeAbsolutePath(entry.path());
    if (child_dir == mesh_dir_normalized) {
      continue;
    }
    collectGltfGlbsInDirectory(child_dir, mesh_absolute, candidates);
  }

  return candidates;
}

}  // namespace Blunder
