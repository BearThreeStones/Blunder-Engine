#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"

#include <cgltf.h>

namespace Blunder {

class MaterialAsset;
class MeshAsset;

/// Draws a mesh with a material at a fixed world matrix (glTF scene import).
struct MeshRendererComponent final {
  eastl::shared_ptr<MeshAsset> mesh;
  eastl::shared_ptr<MaterialAsset> material;
  glm::mat4 world_matrix{1.0f};
  cgltf_alpha_mode alpha_mode{cgltf_alpha_mode_opaque};
  float alpha_cutoff{0.5f};
  bool double_sided{false};
};

}  // namespace Blunder
