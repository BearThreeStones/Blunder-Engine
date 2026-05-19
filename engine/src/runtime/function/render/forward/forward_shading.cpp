#include "runtime/function/render/forward/forward_shading.h"

#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>

#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/resource/asset/material_asset.h"

namespace Blunder {

namespace {

const glm::vec3 k_default_light_direction =
    glm::normalize(glm::vec3(0.45f, 0.7f, 0.55f));

}  // namespace

void applyBlinnPhongToMeshUniforms(ForwardMeshUniformData& mesh_ubo,
                                    const MaterialAsset* material,
                                    const BlinnPhongEditorSettings& editor) {
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
}

}  // namespace Blunder
