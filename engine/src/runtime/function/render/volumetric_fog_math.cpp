#include "runtime/function/render/volumetric_fog_math.h"

#include "EASTL/sort.h"
#include "EASTL/vector.h"

#include "runtime/core/math/geometry.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/light_eval.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

namespace {

/// Fog inject is a windowed range falloff with no `1/r^2`, so only the metre
/// authored `range` needs converting — intensity stays as authored. Never nudge
/// `world_position`: it already comes from `getWorldMatrix` in world units, and
/// promoting it by mesh scale teleports any light parked near the world origin.
void scaleFogLightRangeToWorld(EvaluatedLight& light, bool centimetre_world) {
  light.range = gltfMetreQuantityInWorld(light.range, centimetre_world);
}

}  // namespace

ActiveFog pickActiveFog(const SceneInstance& scene) {
  struct Candidate {
    EntityId id{k_invalid_entity_id};
    FogComponent fog{};
    float world_height_z{0.0f};
  };
  eastl::vector<Candidate> candidates;
  scene.forEachFog([&](EntityId id, const FogComponent& fog) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    if (!fog.enabled || !fog.volumetric_enabled || fog.view_distance <= 0.0f) {
      return;
    }
    Candidate c;
    c.id = id;
    c.fog = fog;
    const Mat4 world = scene.getWorldMatrix(id);
    c.world_height_z = world[3][2];
    candidates.push_back(c);
  });
  eastl::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });
  if (candidates.empty()) {
    return {};
  }
  ActiveFog out;
  out.entity_id = candidates[0].id;
  out.fog = candidates[0].fog;
  out.world_height_z = candidates[0].world_height_z;
  return out;
}

EntityId pickIlluminatingDirectional(const SceneInstance& scene) {
  struct Candidate {
    EntityId id{k_invalid_entity_id};
  };
  eastl::vector<Candidate> candidates;
  scene.forEachLight([&](EntityId id, const LightComponent& light) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    if (!light.enabled || light.type != LightType::directional) {
      return;
    }
    if (!lightContributionIncludesIlluminate(light.contribution)) {
      return;
    }
    candidates.push_back(Candidate{id});
  });
  eastl::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });
  if (candidates.empty()) {
    return k_invalid_entity_id;
  }
  return candidates[0].id;
}

size_t gatherFogLocalLights(const SceneInstance& scene, EvaluatedLight* out_lights,
                            size_t out_capacity) {
  if (out_lights == nullptr || out_capacity == 0) {
    return 0;
  }

  struct Candidate {
    EntityId id{k_invalid_entity_id};
    const LightComponent* light{nullptr};
  };
  eastl::vector<Candidate> candidates;
  scene.forEachLight([&](EntityId id, const LightComponent& light) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    if (!light.enabled || !lightContributionIncludesIlluminate(light.contribution)) {
      return;
    }
    if (light.type != LightType::point && light.type != LightType::spot) {
      return;
    }
    candidates.push_back(Candidate{id, &light});
  });
  eastl::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });

  const size_t count =
      candidates.size() < out_capacity ? candidates.size() : out_capacity;
  for (size_t i = 0; i < count; ++i) {
    fillEvaluatedLight(candidates[i].id, *candidates[i].light,
                       scene.getWorldMatrix(candidates[i].id), out_lights[i]);
  }
  const bool centimetre_world = sceneIsCentimetreWorld(scene);
  for (size_t i = 0; i < count; ++i) {
    scaleFogLightRangeToWorld(out_lights[i], centimetre_world);
  }
  return count;
}

}  // namespace Blunder
