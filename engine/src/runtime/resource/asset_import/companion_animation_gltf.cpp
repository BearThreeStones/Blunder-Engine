#include "runtime/resource/asset_import/companion_animation_gltf.h"

#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <cgltf.h>

#include "runtime/core/base/macro.h"

namespace Blunder {

namespace {

bool isCompanionAnimationGltfDocument(const cgltf_data* data) {
  if (data == nullptr) {
    return false;
  }
  return data->animations_count > 0 && data->meshes_count == 0;
}

bool isSkinnedMeshHostCandidateGltfDocument(const cgltf_data* data) {
  if (data == nullptr) {
    return false;
  }
  return data->skins_count > 0;
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

bool parseGltfDocument(const std::filesystem::path& gltf_absolute,
                       cgltf_data** out_data) {
  if (out_data == nullptr) {
    return false;
  }
  *out_data = nullptr;

  std::vector<std::uint8_t> bytes;
  if (!readFileBytes(gltf_absolute, bytes) || bytes.empty()) {
    return false;
  }

  cgltf_options options{};
  const cgltf_result parse_result =
      cgltf_parse(&options, bytes.data(), bytes.size(), out_data);
  return parse_result == cgltf_result_success && *out_data != nullptr;
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
  cgltf_data* data = nullptr;
  if (!parseGltfDocument(gltf_absolute, &data)) {
    return false;
  }

  const bool accepted = isCompanionAnimationGltfDocument(data);
  cgltf_free(data);
  return accepted;
}

bool isSkinnedMeshHostCandidateGltf(const std::filesystem::path& gltf_absolute) {
  cgltf_data* data = nullptr;
  if (!parseGltfDocument(gltf_absolute, &data)) {
    return false;
  }

  const bool accepted = isSkinnedMeshHostCandidateGltfDocument(data);
  cgltf_free(data);
  return accepted;
}

void warnOnCompanionAnimationBoneMismatches(
    const std::filesystem::path& host_gltf_absolute,
    const std::filesystem::path& companion_gltf_absolute) {
  cgltf_data* host_data = nullptr;
  cgltf_data* companion_data = nullptr;
  if (!parseGltfDocument(host_gltf_absolute, &host_data) ||
      !parseGltfDocument(companion_gltf_absolute, &companion_data)) {
    if (host_data != nullptr) {
      cgltf_free(host_data);
    }
    if (companion_data != nullptr) {
      cgltf_free(companion_data);
    }
    return;
  }

  std::set<std::string> host_bones;
  for (cgltf_size skin_index = 0; skin_index < host_data->skins_count;
       ++skin_index) {
    const cgltf_skin& skin = host_data->skins[skin_index];
    for (cgltf_size joint_index = 0; joint_index < skin.joints_count;
         ++joint_index) {
      const cgltf_node* joint = skin.joints[joint_index];
      if (joint != nullptr && joint->name != nullptr &&
          joint->name[0] != '\0') {
        host_bones.insert(joint->name);
      }
    }
  }

  std::set<std::string> warned_bones;
  for (cgltf_size animation_index = 0;
       animation_index < companion_data->animations_count;
       ++animation_index) {
    const cgltf_animation& animation =
        companion_data->animations[animation_index];
    for (cgltf_size channel_index = 0;
         channel_index < animation.channels_count; ++channel_index) {
      const cgltf_node* target = animation.channels[channel_index].target_node;
      const std::string target_name =
          target != nullptr && target->name != nullptr &&
                  target->name[0] != '\0'
              ? target->name
              : "Node";
      if (host_bones.find(target_name) != host_bones.end() ||
          !warned_bones.insert(target_name).second) {
        continue;
      }
      LOG_WARN(
          "[AssetImport] companion animation bone '{}' is absent from host "
          "skeleton {}; registering clip anyway ({})",
          target_name, host_gltf_absolute.generic_string(),
          companion_gltf_absolute.generic_string());
    }
  }

  cgltf_free(companion_data);
  cgltf_free(host_data);
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

std::vector<std::filesystem::path> discoverAcceptedNearDiskCompanionAnimationGltfs(
    const std::filesystem::path& mesh_gltf_absolute) {
  std::vector<std::filesystem::path> accepted;
  for (const std::filesystem::path& candidate :
       enumerateNearDiskCompanionGltfCandidates(mesh_gltf_absolute)) {
    if (isCompanionAnimationGltf(candidate)) {
      accepted.push_back(candidate);
    }
  }
  return accepted;
}

CompanionGltfMultiSelectBatchPairingResult pairCompanionAnimationGltfMultiSelectBatch(
    const std::vector<std::filesystem::path>& gltf_absolute_paths) {
  CompanionGltfMultiSelectBatchPairingResult result;

  std::vector<std::filesystem::path> host_paths;
  std::vector<std::filesystem::path> companion_paths;

  for (const std::filesystem::path& path : gltf_absolute_paths) {
    if (!isGltfOrGlbExtension(path)) {
      continue;
    }

    const std::filesystem::path absolute = normalizeAbsolutePath(path);
    if (isCompanionAnimationGltf(absolute)) {
      companion_paths.push_back(absolute);
      continue;
    }
    if (isSkinnedMeshHostCandidateGltf(absolute)) {
      host_paths.push_back(absolute);
    }
  }

  if (host_paths.size() == 1) {
    CompanionGltfBatchHostPairing pairing;
    pairing.host_path = host_paths.front();
    pairing.companion_paths = companion_paths;
    result.host_pairings.push_back(std::move(pairing));
    return result;
  }

  for (const std::filesystem::path& host_path : host_paths) {
    CompanionGltfBatchHostPairing pairing;
    pairing.host_path = host_path;
    result.host_pairings.push_back(std::move(pairing));
  }
  result.orphan_companion_paths = std::move(companion_paths);
  return result;
}

}  // namespace Blunder
