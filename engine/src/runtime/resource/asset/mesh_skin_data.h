#pragma once

#include <glm/vec4.hpp>

#include "EASTL/vector.h"

namespace Blunder {

struct MeshSkinInfluence {
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
