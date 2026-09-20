#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/vector_uint4.hpp>

#include <cgltf.h>

#include "runtime/core/math/geometry.h"
#include "runtime/function/render/shadow/virtual_shadow_map.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/gpu_skinning.h"

namespace Blunder {

class MaterialAsset;
class SceneInstance;
struct BlinnPhongEditorSettings;
struct EvaluatedLight;
struct ForwardFrameState;
struct LocalShadowCasters;

constexpr uint32_t k_max_forward_scene_lights = 8;

struct GpuSceneLight {
  glm::vec4 position_type{0.0f};
  glm::vec4 color_flags{0.0f};
  glm::vec4 emit_range{0.0f, 0.0f, -1.0f, 0.0f};
  glm::vec4 cone_area{0.0f};
  glm::vec4 axis_x{1.0f, 0.0f, 0.0f, 0.0f};
};

/// GPU bone palette UBO (std140) shared with skinned shaders; see k_max_gpu_skin_joints.
struct GpuSkinPaletteData {
  glm::mat4 joint_matrices[k_max_gpu_skin_joints];
};

/// Mesh UBO layout shared with engine/shaders/basic.slang / pbr.slang (std140).
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
  glm::vec4 material_flags{0.0f};  // x unlit, y has base-color texture
  glm::mat4 normal_matrix{1.0f};
  glm::mat4 light_view_projection{1.0f};
  glm::vec4 shadow_params{0.0f};
  glm::vec4 metallic_roughness_factors{1.0f, 1.0f, 0.5f, 0.0f};
  glm::vec4 pbr_texture_flags{0.0f};
  glm::uvec4 bindless_texture_indices{0};
  glm::vec4 light_count{0.0f};
  GpuSceneLight lights[k_max_forward_scene_lights];
  ShadowSamplingUniform shadow_sampling{};
};

/// Live scene lighting has no IBL. GPU-driven / deferred both multiply
/// albedo by this floor so a clipped directional map cannot zero the forest.
constexpr float k_live_lighting_ambient_floor = 0.22f;

struct DirectionalShadowPlacement {
  float view_distance{30.0f};
  float near_plane{0.1f};
  float far_plane{60.0f};
  float ortho_half_extent{14.0f};
};

void computeDirectionalLightMatrices(
    glm::vec3 light_dir, glm::vec3 focus, float ortho_half_extent,
    float near_plane, float far_plane, glm::mat4& out_light_view,
    glm::mat4& out_light_projection, glm::mat4& out_light_view_projection,
    float view_distance = 30.0f);

/// Packs one evaluated Light into the GPU layout shared by pbr.slang and
/// deferred_lighting.slang. `axis_x.w` is the shadow code: 0 none, 1 directional
/// VSM/classic, 2+i point cube slot, 10+i spot slot.
void packEvaluatedLight(GpuSceneLight& gpu, const EvaluatedLight& light,
                        EntityId shadow_caster_id, bool shadows_enabled,
                        const LocalShadowCasters* local_shadows = nullptr);

void applyBlinnPhongToMeshUniforms(ForwardMeshUniformData& mesh_ubo,
                                    const MaterialAsset* material,
                                    const BlinnPhongEditorSettings& editor,
                                    const ForwardFrameState& frame_state,
                                    EntityId mesh_entity_id = k_invalid_entity_id);

void applyPbrToMeshUniforms(ForwardMeshUniformData& mesh_ubo,
                            const MaterialAsset* material,
                            const BlinnPhongEditorSettings& editor,
                            const ForwardFrameState& frame_state,
                            cgltf_alpha_mode alpha_mode, float alpha_cutoff,
                            bool double_sided,
                            EntityId mesh_entity_id = k_invalid_entity_id);

float computeShadowOrthoHalfExtentFromAABB(const AABB& bounds,
                                           const glm::vec3& light_direction);

/// Sits the directional ortho camera outside `bounds` so no AABB corner is
/// behind the light or past `far_plane`. The 30/60 courtyard defaults clip a
/// ~175 m forest and shade it black.
DirectionalShadowPlacement computeDirectionalShadowPlacementFromAABB(
    const AABB& bounds, const glm::vec3& light_direction);

}  // namespace Blunder
