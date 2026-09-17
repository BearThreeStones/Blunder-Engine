#include "runtime/function/scene/light_eval.h"

#include <cmath>

#include "EASTL/sort.h"

#include "runtime/core/math/geometry.h"
#include "runtime/function/scene/gltf_unit_scale.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

namespace {

Vec3 safeNormalize(const Vec3& v, const Vec3& fallback) {
  const float len = glm::length(v);
  if (len <= 1e-6f) {
    return fallback;
  }
  return v / len;
}

bool lightIsFullscreenDeferredType(LightType type) {
  return type == LightType::directional || type == LightType::area;
}

}  // namespace

bool sceneIsCentimetreWorld(const SceneInstance& scene) {
  if (!scene.hasWorldBounds()) {
    return false;
  }
  const AABB& bounds = scene.getWorldBounds();
  return looksLikeCentimetreWorldBounds(bounds.min, bounds.max);
}

float sceneWorldUnitsPerMetre(const SceneInstance& scene) {
  return gltfWorldUnitsPerMetre(sceneIsCentimetreWorld(scene));
}

void scaleEvaluatedLightToWorldUnits(EvaluatedLight& light,
                                     float world_units_per_metre) {
  if (world_units_per_metre <= 0.0f ||
      std::fabs(world_units_per_metre - 1.0f) <= 1e-6f ||
      light.type == LightType::directional) {
    return;
  }
  const float s = world_units_per_metre;
  light.range *= s;
  if (light.type == LightType::area) {
    light.width *= s;
    light.height *= s;
  }
  light.color_times_intensity *= s * s;
}

Vec3 lightWorldEmit(const Mat4& world) {
  const Vec3 dir = Vec3(world * Vec4(0.0f, 0.0f, -1.0f, 0.0f));
  return safeNormalize(dir, Vec3(0.0f, 0.0f, -1.0f));
}

Vec3 lightWorldAxisX(const Mat4& world) {
  return safeNormalize(Vec3(world[0]), Vec3(1.0f, 0.0f, 0.0f));
}

Vec3 lightShadingL(LightType type, const Vec3& world_emit,
                   const Vec3& world_position, const Vec3& surface_position) {
  if (type == LightType::point) {
    return safeNormalize(world_position - surface_position, Vec3(0.0f, 0.0f, 1.0f));
  }
  return safeNormalize(-world_emit, Vec3(0.0f, 0.0f, 1.0f));
}

float punctualRangeAttenuation(float distance, float range) {
  if (range <= 1e-6f || distance >= range) {
    return 0.0f;
  }
  const float inv_sq = 1.0f / std::max(distance * distance, 1e-8f);
  const float t = 1.0f - distance / range;
  const float window = t * t;
  return inv_sq * window;
}

bool lightLinkingAffects(const LightComponent& light, EntityId mesh_id) {
  if (light.linking.empty()) {
    return true;
  }
  if (!isValid(mesh_id)) {
    return false;
  }
  for (EntityId linked : light.linking) {
    if (linked == mesh_id) {
      return true;
    }
  }
  return false;
}

bool lightContributionIsNoOpThisSlice(const LightComponent& light) {
  // Area shadows-only has no map this slice. Point/Spot/Directional
  // shadows-only still evaluate so they can occlude.
  if (light.contribution == LightContribution::shadowsOnly &&
      light.type == LightType::area) {
    return true;
  }
  return false;
}

bool lightIsEvaluationCandidate(const LightComponent& light, EntityId mesh_id) {
  if (!light.enabled || lightContributionIsNoOpThisSlice(light)) {
    return false;
  }
  return lightLinkingAffects(light, mesh_id);
}

void fillEvaluatedLight(EntityId entity_id, const LightComponent& light,
                        const Mat4& world, EvaluatedLight& out_light) {
  out_light.entity_id = entity_id;
  out_light.type = light.type;
  out_light.contribution = light.contribution;
  out_light.world_position = Vec3(world[3]);
  out_light.world_emit = lightWorldEmit(world);
  out_light.world_axis_x = lightWorldAxisX(world);
  out_light.color_times_intensity = light.color * light.intensity;
  out_light.range = light.range;
  out_light.inner_cone_degrees = light.inner_cone_degrees;
  out_light.outer_cone_degrees = light.outer_cone_degrees;
  out_light.width = light.width;
  out_light.height = light.height;
}

size_t gatherLightsForMesh(const SceneInstance& scene, EntityId mesh_id,
                           EvaluatedLight* out_lights, size_t out_capacity) {
  if (out_lights == nullptr || out_capacity == 0) {
    return 0;
  }

  struct Candidate {
    EntityId id;
    const LightComponent* light;
  };
  eastl::vector<Candidate> candidates;
  scene.forEachLight([&](EntityId id, const LightComponent& light) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    if (!lightIsEvaluationCandidate(light, mesh_id)) {
      return;
    }
    candidates.push_back(Candidate{id, &light});
  });
  eastl::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });

  const size_t count =
      candidates.size() < out_capacity ? candidates.size() : out_capacity;
  const float world_per_metre = sceneWorldUnitsPerMetre(scene);
  for (size_t i = 0; i < count; ++i) {
    fillEvaluatedLight(candidates[i].id, *candidates[i].light,
                       scene.getWorldMatrix(candidates[i].id), out_lights[i]);
    scaleEvaluatedLightToWorldUnits(out_lights[i], world_per_metre);
  }
  return count;
}

bool lightIsDeferredListCandidate(const LightComponent& light) {
  return light.enabled && !lightContributionIsNoOpThisSlice(light);
}

size_t buildDeferredLightList(const SceneInstance& scene,
                               EvaluatedLight* out_lights, size_t out_capacity) {
  if (out_lights == nullptr || out_capacity == 0) {
    return 0;
  }

  struct Candidate {
    EntityId id;
    const LightComponent* light;
  };
  eastl::vector<Candidate> candidates;
  scene.forEachLight([&](EntityId id, const LightComponent& light) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    if (!lightIsDeferredListCandidate(light)) {
      return;
    }
    candidates.push_back(Candidate{id, &light});
  });
  eastl::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });

  const size_t count =
      candidates.size() < out_capacity ? candidates.size() : out_capacity;
  const float world_per_metre = sceneWorldUnitsPerMetre(scene);
  for (size_t i = 0; i < count; ++i) {
    fillEvaluatedLight(candidates[i].id, *candidates[i].light,
                       scene.getWorldMatrix(candidates[i].id), out_lights[i]);
    scaleEvaluatedLightToWorldUnits(out_lights[i], world_per_metre);
  }
  return count;
}

size_t buildDeferredFullscreenLightList(const SceneInstance& scene,
                                        EvaluatedLight* out_lights,
                                        size_t out_capacity) {
  if (out_lights == nullptr || out_capacity == 0) {
    return 0;
  }

  struct Candidate {
    EntityId id;
    const LightComponent* light;
  };
  eastl::vector<Candidate> candidates;
  scene.forEachLight([&](EntityId id, const LightComponent& light) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    if (!lightIsDeferredListCandidate(light)) {
      return;
    }
    if (!lightIsFullscreenDeferredType(light.type)) {
      return;
    }
    candidates.push_back(Candidate{id, &light});
  });
  eastl::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });

  const size_t count =
      candidates.size() < out_capacity ? candidates.size() : out_capacity;
  const float world_per_metre = sceneWorldUnitsPerMetre(scene);
  for (size_t i = 0; i < count; ++i) {
    fillEvaluatedLight(candidates[i].id, *candidates[i].light,
                       scene.getWorldMatrix(candidates[i].id), out_lights[i]);
    scaleEvaluatedLightToWorldUnits(out_lights[i], world_per_metre);
  }
  return count;
}

size_t evaluateLightsForReceiver(const SceneInstance& scene, EntityId mesh_id,
                                 const EvaluatedLight* list, size_t list_count,
                                 EvaluatedLight* out_lights,
                                 size_t out_capacity) {
  if (out_lights == nullptr || out_capacity == 0 || list == nullptr) {
    return 0;
  }

  size_t written = 0;
  for (size_t i = 0; i < list_count && written < out_capacity; ++i) {
    const LightComponent* light = scene.getLight(list[i].entity_id);
    if (light == nullptr) {
      continue;
    }
    if (!lightLinkingAffects(*light, mesh_id)) {
      continue;
    }
    out_lights[written] = list[i];
    ++written;
  }
  return written;
}

EntityId pickDirectionalShadowCaster(const SceneInstance& scene) {
  struct Candidate {
    EntityId id;
  };
  eastl::vector<Candidate> candidates;
  scene.forEachLight([&](EntityId id, const LightComponent& light) {
    if (!scene.isActiveInHierarchy(id)) {
      return;
    }
    if (!light.enabled || light.type != LightType::directional) {
      return;
    }
    if (!lightContributionIncludesShadows(light.contribution)) {
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

}  // namespace Blunder
