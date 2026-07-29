#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "EASTL/vector.h"

namespace Blunder {

struct MeshSkinInfluence {
  glm::ivec4 joint_indices{0, 0, 0, 0};
  glm::vec4 weights{1.0f, 0.0f, 0.0f, 0.0f};
};

/// Bind-pose vertex plus joint influences for GPU skinning draw.
struct SkinnedMeshVertex {
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 0.0f, 1.0f};
  glm::vec2 uv{0.0f};
  glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
  glm::ivec4 joint_indices{0, 0, 0, 0};
  glm::vec4 weights{1.0f, 0.0f, 0.0f, 0.0f};
};

/// Per-vertex skin influences plus glTF skin joint slot → Skeleton bone index map.
struct MeshSkinData {
  eastl::vector<MeshSkinInfluence> influences;
  eastl::vector<int> joint_to_bone;

  bool isValid() const {
    return !influences.empty() && !joint_to_bone.empty();
  }
};

}  // namespace Blunder
