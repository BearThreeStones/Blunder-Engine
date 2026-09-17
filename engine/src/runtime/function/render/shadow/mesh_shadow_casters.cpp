#include "runtime/function/render/shadow/mesh_shadow_casters.h"

#include "EASTL/sort.h"

#include "runtime/function/render/gpu_mesh.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

bool meshShadowDrawIsOpaqueCaster(const GpuDrivenDraw& draw) {
  // GPU-driven draws already exclude skinned palettes and transparent/blend
  // meshes; foliage is not a caster type this slice. Alpha-mask still arrives
  // on the GPU-driven list and must be skipped here.
  if (draw.gpu_mesh == nullptr || !draw.gpu_mesh->hasMeshlets()) {
    return false;
  }
  if (draw.alpha_mode != cgltf_alpha_mode_opaque) {
    return false;
  }
  return true;
}

void collectOpaqueMeshletCasters(const GpuDrivenDraw* draws, uint32_t count,
                                 eastl::vector<MeshShadowCasterDraw>& out_casters) {
  out_casters.clear();
  if (draws == nullptr || count == 0) {
    return;
  }
  for (uint32_t i = 0; i < count; ++i) {
    const GpuDrivenDraw& draw = draws[i];
    if (!meshShadowDrawIsOpaqueCaster(draw)) {
      continue;
    }
    MeshShadowCasterDraw caster{};
    caster.draw = &draw;
    caster.meshlet_count =
        static_cast<uint32_t>(draw.gpu_mesh->getMeshletRecords().size());
    out_casters.push_back(caster);
  }
}

void filterCastersForLight(const eastl::vector<MeshShadowCasterDraw>& casters,
                           const LightComponent* light,
                           eastl::vector<MeshShadowCasterDraw>& out_filtered) {
  out_filtered.clear();
  for (const MeshShadowCasterDraw& caster : casters) {
    if (caster.draw == nullptr) {
      continue;
    }
    if (light != nullptr &&
        !lightLinkingAffects(*light, caster.draw->entity_id)) {
      continue;
    }
    out_filtered.push_back(caster);
  }
}

LocalShadowCasters pickLocalShadowCasters(const SceneInstance& scene) {
  LocalShadowCasters result{};
  result.directional = pickDirectionalShadowCaster(scene);

  struct Candidate {
    EntityId id;
  };
  eastl::vector<Candidate> points;
  eastl::vector<Candidate> spots;
  scene.forEachLight([&](EntityId id, const LightComponent& light) {
    if (!scene.isActiveInHierarchy(id) || !light.enabled) {
      return;
    }
    if (!lightContributionIncludesShadows(light.contribution)) {
      return;
    }
    if (light.type == LightType::point) {
      points.push_back(Candidate{id});
    } else if (light.type == LightType::spot) {
      spots.push_back(Candidate{id});
    }
  });
  eastl::sort(points.begin(), points.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });
  eastl::sort(spots.begin(), spots.end(),
              [](const Candidate& a, const Candidate& b) { return a.id < b.id; });

  for (const Candidate& c : points) {
    if (result.point_count < k_max_point_shadow_maps) {
      result.points[result.point_count++] = c.id;
    } else {
      ++result.point_dropped;
    }
  }
  for (const Candidate& c : spots) {
    if (result.spot_count < k_max_spot_shadow_maps) {
      result.spots[result.spot_count++] = c.id;
    } else {
      ++result.spot_dropped;
    }
  }
  return result;
}

int32_t shadowSlotForPoint(const LocalShadowCasters& casters, EntityId id) {
  if (!isValid(id)) {
    return -1;
  }
  for (uint32_t i = 0; i < casters.point_count; ++i) {
    if (casters.points[i] == id) {
      return static_cast<int32_t>(i);
    }
  }
  return -1;
}

int32_t shadowSlotForSpot(const LocalShadowCasters& casters, EntityId id) {
  if (!isValid(id)) {
    return -1;
  }
  for (uint32_t i = 0; i < casters.spot_count; ++i) {
    if (casters.spots[i] == id) {
      return static_cast<int32_t>(i);
    }
  }
  return -1;
}

float shadowCodeForLight(const EvaluatedLight& light, EntityId directional_id,
                         const LocalShadowCasters& casters,
                         bool shadows_enabled) {
  if (!shadows_enabled) {
    return k_shadow_code_none;
  }
  if (light.type == LightType::directional &&
      isValid(directional_id) && light.entity_id == directional_id) {
    return k_shadow_code_directional;
  }
  if (light.type == LightType::point) {
    const int32_t slot = shadowSlotForPoint(casters, light.entity_id);
    if (slot >= 0) {
      return k_shadow_code_point_base + static_cast<float>(slot);
    }
  }
  if (light.type == LightType::spot) {
    const int32_t slot = shadowSlotForSpot(casters, light.entity_id);
    if (slot >= 0) {
      return k_shadow_code_spot_base + static_cast<float>(slot);
    }
  }
  return k_shadow_code_none;
}

}  // namespace Blunder
