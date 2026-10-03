#include "runtime/function/render/shadow/mesh_shadow_casters.h"

#include "EASTL/algorithm.h"
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

uint32_t packShadowCasterBatches(eastl::vector<MeshShadowCasterDraw>& casters,
                                 uint32_t max_instances,
                                 uint32_t max_unique_meshlets,
                                 uint32_t max_batches,
                                 eastl::vector<ShadowMeshBatch>& out_batches) {
  out_batches.clear();
  uint32_t kept = 0;
  for (uint32_t src = 0; src < casters.size(); ++src) {
    const MeshShadowCasterDraw& caster = casters[src];
    if (caster.draw == nullptr || caster.draw->gpu_mesh == nullptr) {
      continue;
    }
    if (kept != src) {
      casters[kept] = caster;
    }
    ++kept;
  }
  casters.resize(kept);
  eastl::sort(casters.begin(), casters.end(),
              [](const MeshShadowCasterDraw& a, const MeshShadowCasterDraw& b) {
                if (a.draw->gpu_mesh != b.draw->gpu_mesh) {
                  return a.draw->gpu_mesh < b.draw->gpu_mesh;
                }
                return a.draw->entity_id < b.draw->entity_id;
              });
  if (max_instances == 0 || max_batches == 0) {
    return 0;
  }

  uint32_t packed = 0;
  uint32_t unique_meshlets = 0;
  for (const MeshShadowCasterDraw& caster : casters) {
    if (packed >= max_instances) {
      break;
    }
    const bool new_batch = out_batches.empty() ||
                           out_batches.back().mesh != caster.draw->gpu_mesh;
    if (new_batch) {
      if (out_batches.size() >= max_batches) {
        break;
      }
      if (caster.meshlet_count > max_unique_meshlets) {
        break;
      }
      if (unique_meshlets > max_unique_meshlets - caster.meshlet_count) {
        break;
      }
      ShadowMeshBatch batch{};
      batch.mesh = caster.draw->gpu_mesh;
      batch.instance_first = packed;
      batch.instance_count = 0;
      batch.meshlet_offset = unique_meshlets;
      batch.meshlet_count = caster.meshlet_count;
      out_batches.push_back(batch);
      unique_meshlets += caster.meshlet_count;
    }
    out_batches.back().instance_count += 1u;
    ++packed;
  }
  return packed;
}

void buildShadowMeshTaskDispatches(uint32_t meshlets_per_instance,
                                   uint32_t instance_count, uint32_t face_mul,
                                   uint32_t max_groups_per_dim,
                                   uint32_t max_groups_total,
                                   eastl::vector<ShadowMeshTaskDispatch>& out) {
  out.clear();
  if (meshlets_per_instance == 0 || instance_count == 0) {
    return;
  }
  if (face_mul == 0) {
    face_mul = 1u;
  }
  if (max_groups_per_dim == 0) {
    max_groups_per_dim = 1u;
  }
  if (max_groups_total == 0) {
    max_groups_total = 1u;
  }
  const uint32_t meshlets_per_dispatch =
      eastl::max(1u, max_groups_per_dim / face_mul);
  for (uint32_t meshlet_base = 0; meshlet_base < meshlets_per_instance;
       meshlet_base += meshlets_per_dispatch) {
    const uint32_t meshlet_count = eastl::min(
        meshlets_per_dispatch, meshlets_per_instance - meshlet_base);
    // face_mul <= max_groups_per_dim keeps X inside the per-axis limit.
    const uint32_t groups_x = meshlet_count * face_mul;
    uint32_t instances_per_dispatch = eastl::min(instance_count, max_groups_per_dim);
    if (groups_x > 0) {
      const uint32_t by_total = eastl::max(1u, max_groups_total / groups_x);
      instances_per_dispatch = eastl::min(instances_per_dispatch, by_total);
    }
    for (uint32_t instance_base = 0; instance_base < instance_count;
         instance_base += instances_per_dispatch) {
      ShadowMeshTaskDispatch dispatch{};
      dispatch.group_count_x = groups_x;
      dispatch.group_count_y =
          eastl::min(instances_per_dispatch, instance_count - instance_base);
      dispatch.instance_first = instance_base;
      dispatch.meshlet_base = meshlet_base;
      dispatch.meshlet_count = meshlet_count;
      out.push_back(dispatch);
    }
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
