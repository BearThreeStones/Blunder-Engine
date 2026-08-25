#pragma once

#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/light_component.h"

namespace Blunder {

class SceneInstance;

constexpr size_t k_max_evaluated_lights_per_mesh = 8;

struct EvaluatedLight final {
  EntityId entity_id{k_invalid_entity_id};
  LightType type{LightType::directional};
  LightContribution contribution{LightContribution::illuminateAndShadows};
  Vec3 world_position{0.0f};
  Vec3 world_emit{0.0f, 0.0f, -1.0f};
  Vec3 world_axis_x{1.0f, 0.0f, 0.0f};
  Vec3 color_times_intensity{1.0f};
  float range{10.0f};
  float inner_cone_degrees{0.0f};
  float outer_cone_degrees{45.0f};
  float width{1.0f};
  float height{1.0f};
};

Vec3 lightWorldEmit(const Mat4& world);
Vec3 lightWorldAxisX(const Mat4& world);
Vec3 lightShadingL(LightType type, const Vec3& world_emit,
                   const Vec3& world_position, const Vec3& surface_position);
float punctualRangeAttenuation(float distance, float range);

bool lightLinkingAffects(const LightComponent& light, EntityId mesh_id);
bool lightContributionIsNoOpThisSlice(const LightComponent& light);
bool lightIsEvaluationCandidate(const LightComponent& light, EntityId mesh_id);

void fillEvaluatedLight(EntityId entity_id, const LightComponent& light,
                        const Mat4& world, EvaluatedLight& out_light);

size_t gatherLightsForMesh(const SceneInstance& scene, EntityId mesh_id,
                           EvaluatedLight* out_lights, size_t out_capacity);

EntityId pickDirectionalShadowCaster(const SceneInstance& scene);

}  // namespace Blunder
