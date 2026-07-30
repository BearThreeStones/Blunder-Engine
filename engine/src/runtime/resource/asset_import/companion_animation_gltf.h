#pragma once

#include <filesystem>

namespace Blunder {

/// Returns true when `gltf_absolute` is a Companion Animation glTF per ADR 0021:
/// one or more animations and zero meshes (skins allowed).
bool isCompanionAnimationGltf(const std::filesystem::path& gltf_absolute);

}  // namespace Blunder
