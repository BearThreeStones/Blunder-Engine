#include "runtime/function/scene/light_eval.h"

#include "EASTL/sort.h"

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

}  // namespace

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
  if (light.contribution == LightContribution::shadowsOnly &&
      light.type != LightType::directional) {
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
  for (size_t i = 0; i < count; ++i) {
    fillEvaluatedLight(candidates[i].id, *candidates[i].light,
                       scene.getWorldMatrix(candidates[i].id), out_lights[i]);
  }
  return count;
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
