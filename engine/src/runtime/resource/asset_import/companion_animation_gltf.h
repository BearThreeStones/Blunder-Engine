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

}  // namespace Blunder
