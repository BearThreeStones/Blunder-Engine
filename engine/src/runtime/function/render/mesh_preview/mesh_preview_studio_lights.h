#pragma once

#include <glm/vec3.hpp>

namespace Blunder {

/// Fixed studio lighting for Mesh Preview (not active scene light rig).
struct MeshPreviewStudioLights {
  glm::vec3 key_light_direction{0.45f, 0.7f, 0.55f};
  glm::vec3 key_light_color{1.0f};
  glm::vec3 fill_light_direction{-0.35f, 0.25f, 0.55f};
  glm::vec3 fill_light_color{0.35f};
  glm::vec3 ambient_color{0.28f};
  bool shadows_enabled{false};
};

inline MeshPreviewStudioLights defaultMeshPreviewStudioLights() {
  return MeshPreviewStudioLights{};
}

}  // namespace Blunder
