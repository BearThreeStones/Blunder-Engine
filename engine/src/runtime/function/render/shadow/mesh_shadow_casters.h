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
