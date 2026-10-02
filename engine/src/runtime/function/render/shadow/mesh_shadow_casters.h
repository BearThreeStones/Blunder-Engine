#pragma once

#include <cstdint>

#include "EASTL/vector.h"

#include "runtime/function/render/gpu_driven/gpu_driven_types.h"
#include "runtime/function/render/shadow/virtual_shadow_map.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/light_eval.h"

namespace Blunder {

class SceneInstance;
struct GpuDrivenDraw;

struct MeshShadowCasterDraw {
  const GpuDrivenDraw* draw{nullptr};
  uint32_t meshlet_count{0};
};

bool meshShadowDrawIsOpaqueCaster(const GpuDrivenDraw& draw);

void collectOpaqueMeshletCasters(const GpuDrivenDraw* draws, uint32_t count,
                                 eastl::vector<MeshShadowCasterDraw>& out_casters);

void filterCastersForLight(const eastl::vector<MeshShadowCasterDraw>& casters,
                           const LightComponent* light,
                           eastl::vector<MeshShadowCasterDraw>& out_filtered);

/// One shadow-fill batch: every caster that shares a `GpuMesh`. Meshlet
/// records are stored once per batch; instances keep their own world matrices.
struct ShadowMeshBatch {
  GpuMesh* mesh{nullptr};
  uint32_t instance_first{0};
  uint32_t instance_count{0};
  uint32_t meshlet_offset{0};
  uint32_t meshlet_count{0};
};

/// Vulkan `vkCmdDrawMeshTasksEXT` minimums (one dimension, and X*Y*Z).
inline constexpr uint32_t k_shadow_mesh_task_group_limit = 65535u;
inline constexpr uint32_t k_shadow_mesh_task_group_total_limit = 1u << 22;

/// Sorts `casters` by `GpuMesh*` then entity id and packs a prefix into
/// unique-mesh batches. Meshlet budget counts each mesh once, not once per
/// caster. Returns how many leading casters were packed. Does not drop the
/// unpacked tail from `casters` (the VS fallback still walks the full list).
uint32_t packShadowCasterBatches(eastl::vector<MeshShadowCasterDraw>& casters,
                                 uint32_t max_instances,
                                 uint32_t max_unique_meshlets,
                                 uint32_t max_batches,
                                 eastl::vector<ShadowMeshBatch>& out_batches);

/// One `vkCmdDrawMeshTasksEXT` covering a slice of a mesh batch.
/// X = meshlets in the slice × face count, Y = instances in the slice.
struct ShadowMeshTaskDispatch {
  uint32_t group_count_x{0};
  uint32_t group_count_y{0};
  uint32_t instance_first{0};
  uint32_t meshlet_base{0};
  uint32_t meshlet_count{0};
};

void buildShadowMeshTaskDispatches(uint32_t meshlets_per_instance,
                                   uint32_t instance_count, uint32_t face_mul,
                                   uint32_t max_groups_per_dim,
                                   uint32_t max_groups_total,
                                   eastl::vector<ShadowMeshTaskDispatch>& out);

struct LocalShadowCasters {
  EntityId directional{k_invalid_entity_id};
  EntityId points[k_max_point_shadow_maps]{};
  uint32_t point_count{0};
  uint32_t point_dropped{0};
  EntityId spots[k_max_spot_shadow_maps]{};
  uint32_t spot_count{0};
  uint32_t spot_dropped{0};
};

LocalShadowCasters pickLocalShadowCasters(const SceneInstance& scene);

int32_t shadowSlotForPoint(const LocalShadowCasters& casters, EntityId id);
int32_t shadowSlotForSpot(const LocalShadowCasters& casters, EntityId id);
float shadowCodeForLight(const EvaluatedLight& light, EntityId directional_id,
                         const LocalShadowCasters& casters, bool shadows_enabled);

}  // namespace Blunder
