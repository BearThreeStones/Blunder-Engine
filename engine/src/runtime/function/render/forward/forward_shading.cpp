#include "runtime/function/render/forward/forward_shading.h"

#include <cmath>
#include <limits>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/core/math/geometry.h"

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

  glm::vec3 up = kWorldUp;
  if (std::abs(glm::dot(normalized_light_dir, up)) > 0.95f) {
    up = kWorldForward;
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
  const glm::vec3 ambient =
      glm::max(editor.ambient_color, glm::vec3(0.25f));
  mesh_ubo.ambient_color = glm::vec4(ambient, 0.0f);
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

void applyPbrToMeshUniforms(ForwardMeshUniformData& mesh_ubo,
                              const MaterialAsset* material,
                              const BlinnPhongEditorSettings& editor,
                              const ForwardFrameState& frame_state,
                              cgltf_alpha_mode alpha_mode, float alpha_cutoff,
                              bool double_sided) {
  applyBlinnPhongToMeshUniforms(mesh_ubo, material, editor, frame_state);

  float metallic = 1.0f;
  float roughness = 1.0f;
  if (material != nullptr) {
    metallic = material->getMetallicFactor();
    roughness = material->getRoughnessFactor();
    mesh_ubo.material_flags.x = material->isUnlit() ? 1.0f : 0.0f;
  }

  mesh_ubo.metallic_roughness_factors =
      glm::vec4(metallic, roughness, alpha_cutoff,
                static_cast<float>(static_cast<int>(alpha_mode)));
  mesh_ubo.pbr_texture_flags = glm::vec4(
      material != nullptr && material->hasMetallicRoughnessTexture() ? 1.0f : 0.0f,
      material != nullptr && material->hasNormalTexture() ? 1.0f : 0.0f,
      material != nullptr && material->hasOcclusionTexture() ? 1.0f : 0.0f,
      double_sided ? 1.0f : 0.0f);
}

float computeShadowOrthoHalfExtentFromAABB(const AABB& bounds,
                                         const glm::vec3& light_direction) {
  const glm::vec3 light_dir =
      glm::length(light_direction) > 0.0001f
          ? glm::normalize(light_direction)
          : k_default_light_direction;

  glm::vec3 tangent = glm::cross(light_dir, kWorldUp);
  if (glm::dot(tangent, tangent) < 0.0001f) {
    tangent = glm::cross(light_dir, kWorldForward);
  }
  tangent = glm::normalize(tangent);
  const glm::vec3 bitangent = glm::normalize(glm::cross(light_dir, tangent));

  const glm::vec3 corners[8] = {
      {bounds.min.x, bounds.min.y, bounds.min.z},
      {bounds.max.x, bounds.min.y, bounds.min.z},
      {bounds.min.x, bounds.max.y, bounds.min.z},
      {bounds.max.x, bounds.max.y, bounds.min.z},
      {bounds.min.x, bounds.min.y, bounds.max.z},
      {bounds.max.x, bounds.min.y, bounds.max.z},
      {bounds.min.x, bounds.max.y, bounds.max.z},
      {bounds.max.x, bounds.max.y, bounds.max.z},
  };

  float min_u = std::numeric_limits<float>::max();
  float max_u = std::numeric_limits<float>::lowest();
  float min_v = std::numeric_limits<float>::max();
  float max_v = std::numeric_limits<float>::lowest();
  for (const glm::vec3& corner : corners) {
    const float u = glm::dot(corner, tangent);
    const float v = glm::dot(corner, bitangent);
    min_u = std::min(min_u, u);
    max_u = std::max(max_u, u);
    min_v = std::min(min_v, v);
    max_v = std::max(max_v, v);
  }

  const float half_u = (max_u - min_u) * 0.5f;
  const float half_v = (max_v - min_v) * 0.5f;
  const float radius = std::sqrt(half_u * half_u + half_v * half_v);
  return std::max(radius * 1.05f, 2.0f);
}

}  // namespace Blunder
