#include "runtime/function/render/forward/forward_shading.h"

#include <cmath>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>

#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/shadow/shadow_map_target.h"
#include "runtime/resource/asset/material_asset.h"

namespace Blunder {

namespace {

const glm::vec3 k_default_light_direction =
    glm::normalize(glm::vec3(0.45f, 0.7f, 0.55f));

constexpr float k_light_view_distance = 30.0f;

}  // namespace

void computeDirectionalLightMatrices(
    glm::vec3 light_dir, glm::vec3 focus, float ortho_half_extent,
    float near_plane, float far_plane, glm::mat4& out_light_view,
    glm::mat4& out_light_projection, glm::mat4& out_light_view_projection) {
  const float light_dir_length = glm::length(light_dir);
  const glm::vec3 normalized_light_dir =
      light_dir_length > 0.0001f ? light_dir / light_dir_length
                                 : k_default_light_direction;

  glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
  if (std::abs(glm::dot(normalized_light_dir, up)) > 0.95f) {
    up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  const glm::vec3 light_position =
      focus - normalized_light_dir * k_light_view_distance;
  out_light_view = glm::lookAt(light_position, focus, up);
  out_light_projection = glm::ortho(-ortho_half_extent, ortho_half_extent,
                                    -ortho_half_extent, ortho_half_extent,
                                    near_plane, far_plane);
  out_light_projection[1][1] *= -1.0f;
  out_light_view_projection = out_light_projection * out_light_view;
}

void applyBlinnPhongToMeshUniforms(ForwardMeshUniformData& mesh_ubo,
                                    const MaterialAsset* material,
                                    const BlinnPhongEditorSettings& editor,
                                    const ForwardFrameState& frame_state) {
  if (material != nullptr) {
    mesh_ubo.base_color_factor = material->getBaseColorFactor();
  } else {
    mesh_ubo.base_color_factor = glm::vec4(1.0f);
  }

  const float light_dir_length = glm::length(editor.light_direction);
  const glm::vec3 light_dir =
      light_dir_length > 0.0001f
          ? editor.light_direction / light_dir_length
          : k_default_light_direction;
  mesh_ubo.light_direction = glm::vec4(light_dir, 0.0f);
  mesh_ubo.light_color = glm::vec4(editor.light_color, 0.0f);
  mesh_ubo.ambient_color = glm::vec4(editor.ambient_color, 0.0f);
  mesh_ubo.diffuse_color = glm::vec4(editor.diffuse_color, 0.0f);
  mesh_ubo.specular_color_and_shininess =
      glm::vec4(editor.specular_color, editor.shininess);
  mesh_ubo.material_flags =
      glm::vec4(editor.unlit ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

  mesh_ubo.light_view_projection = frame_state.light_view_projection;
  const float inv_shadow_map_size =
      1.0f / static_cast<float>(ShadowMapTarget::k_shadow_map_size);
  mesh_ubo.shadow_params = glm::vec4(
      frame_state.shadow_bias, frame_state.shadows_enabled ? 1.0f : 0.0f,
      inv_shadow_map_size, 0.0f);
}

}  // namespace Blunder
