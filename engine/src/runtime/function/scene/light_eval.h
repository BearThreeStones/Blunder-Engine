#pragma once

#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/light_component.h"

namespace Blunder {

class SceneInstance;

constexpr size_t k_max_evaluated_lights_per_mesh = 8;
constexpr size_t k_max_deferred_light_list = 32;

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

/// One place decides mesh unit scale. Overlays, fog, shadows and shading all
/// read it here; three private copies of this predicate is how metre-authored
/// ranges ended up compared against centimetre distances.
bool sceneIsCentimetreWorld(const SceneInstance& scene);
/// Overlay: mesh *vertex* AABB can be centimetre even when scene world bounds
/// were never set (or were Unique metres). Drawn Sponza is this, not Unique world.
bool sceneDrawnMeshIsCentimetre(const SceneInstance& scene);
float sceneWorldUnitsPerMetre(const SceneInstance& scene);

/// Punctual `range`, area extent and the `1/d^2` term are authored in metres but
/// shaded against world-space distances. Convert the lengths and let intensity
/// carry the square so `1/d_world^2 * intensity` still equals `1/d_metre^2`.
/// Directional light has neither range nor falloff, so it is left alone.
void scaleEvaluatedLightToWorldUnits(EvaluatedLight& light,
                                     float world_units_per_metre);

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

size_t buildDeferredLightList(const SceneInstance& scene,
                               EvaluatedLight* out_lights, size_t out_capacity);

/// Directional/area only, filtered before `out_capacity` (fullscreen deferred UBO).
size_t buildDeferredFullscreenLightList(const SceneInstance& scene,
                                        EvaluatedLight* out_lights,
                                        size_t out_capacity);

size_t evaluateLightsForReceiver(const SceneInstance& scene, EntityId mesh_id,
                                  const EvaluatedLight* list, size_t list_count,
                                  EvaluatedLight* out_lights,
                                  size_t out_capacity);

EntityId pickDirectionalShadowCaster(const SceneInstance& scene);

}  // namespace Blunder
