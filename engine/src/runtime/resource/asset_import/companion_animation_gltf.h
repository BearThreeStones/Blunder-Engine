#pragma once

#include <filesystem>
#include <vector>

namespace Blunder {

/// Returns true when `gltf_absolute` is a Companion Animation glTF per ADR 0021:
/// one or more animations and zero meshes (skins allowed).
bool isCompanionAnimationGltf(const std::filesystem::path& gltf_absolute);

/// Near-disk companion discovery (ADR 0021): glTF/GLB paths in the mesh file's
/// directory and in immediate child directories of the mesh's parent directory.
/// Does not recurse deeper; does not walk unrelated trees. Excludes `mesh_gltf_absolute`.
std::vector<std::filesystem::path> enumerateNearDiskCompanionGltfCandidates(
    const std::filesystem::path& mesh_gltf_absolute);

/// Near-disk candidates filtered by Companion Animation acceptance (ADR 0021).
std::vector<std::filesystem::path> discoverAcceptedNearDiskCompanionAnimationGltfs(
    const std::filesystem::path& mesh_gltf_absolute);

/// Returns true when `gltf_absolute` is a skinned mesh host candidate (skins present).
bool isSkinnedMeshHostCandidateGltf(const std::filesystem::path& gltf_absolute);

/// Warns for animation target bones absent from the host skin. Parse failures
/// are left to the importer/extractor; mismatches do not reject the companion.
void warnOnCompanionAnimationBoneMismatches(
    const std::filesystem::path& host_gltf_absolute,
    const std::filesystem::path& companion_gltf_absolute);

struct CompanionGltfBatchHostPairing {
  std::filesystem::path host_path;
  std::vector<std::filesystem::path> companion_paths;
};

struct CompanionGltfMultiSelectBatchPairingResult {
  /// One entry per skinned mesh host. Exactly one host includes companions; multiple
  /// hosts each get an entry with empty companions.
  std::vector<CompanionGltfBatchHostPairing> host_pairings;
  /// Companion-accepted glTFs with no single unambiguous host (caller may log).
  std::vector<std::filesystem::path> orphan_companion_paths;
};

/// Multi-select batch pairing (ADR 0021): classify glTF/GLB paths into skinned mesh
/// hosts and companion animations. Non-glTF paths are ignored.
CompanionGltfMultiSelectBatchPairingResult pairCompanionAnimationGltfMultiSelectBatch(
    const std::vector<std::filesystem::path>& gltf_absolute_paths);

}  // namespace Blunder
