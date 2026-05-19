#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace Blunder {

class MaterialAsset;
struct BlinnPhongEditorSettings;
struct ForwardFrameState;

/// Mesh UBO layout shared with engine/shaders/basic.slang (std140).
struct ForwardMeshUniformData {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec4 camera_position{0.0f};
  glm::vec4 light_direction{0.45f, 0.7f, 0.55f, 0.0f};
  glm::vec4 light_color{1.0f};
  glm::vec4 base_color_factor{1.0f};
  glm::vec4 ambient_color{0.15f, 0.15f, 0.15f, 0.0f};
  glm::vec4 diffuse_color{0.85f, 0.85f, 0.85f, 0.0f};
  glm::vec4 specular_color_and_shininess{0.4f, 0.4f, 0.4f, 32.0f};
  glm::vec4 material_flags{0.0f};
  glm::mat4 normal_matrix{1.0f};
  glm::mat4 light_view_projection{1.0f};
  glm::vec4 shadow_params{0.0f};
};

void computeDirectionalLightMatrices(
    glm::vec3 light_dir, glm::vec3 focus, float ortho_half_extent,
    float near_plane, float far_plane, glm::mat4& out_light_view,
    glm::mat4& out_light_projection, glm::mat4& out_light_view_projection);

void applyBlinnPhongToMeshUniforms(ForwardMeshUniformData& mesh_ubo,
                                    const MaterialAsset* material,
                                    const BlinnPhongEditorSettings& editor,
                                    const ForwardFrameState& frame_state);

}  // namespace Blunder
