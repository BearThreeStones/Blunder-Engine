#pragma once

#include <glm/vec3.hpp>

namespace Blunder {

/// Live Blinn-Phong parameters edited from the Slint inspector panel.
struct BlinnPhongEditorSettings {
  glm::vec3 light_direction{0.45f, 0.7f, 0.55f};
  glm::vec3 light_color{1.0f};
  glm::vec3 ambient_color{0.28f};
  glm::vec3 diffuse_color{1.0f};
  glm::vec3 specular_color{0.4f};
  float shininess{32.0f};
  bool unlit{false};

  bool ssao_enabled{false};
  float ssao_radius{0.45f};
  float ssao_bias{0.025f};
  float ssao_strength{0.5f};
};

}  // namespace Blunder
