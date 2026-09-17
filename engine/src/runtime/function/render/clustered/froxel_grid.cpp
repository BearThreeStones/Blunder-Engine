#include "runtime/function/render/clustered/froxel_grid.h"

#include "EASTL/sort.h"
#include "EASTL/vector.h"

#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

size_t buildClusteredPointSpotList(const SceneInstance& scene,
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
    if (!light.enabled || lightContributionIsNoOpThisSlice(light)) {
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
  const float world_per_metre = sceneWorldUnitsPerMetre(scene);
  for (size_t i = 0; i < count; ++i) {
    fillEvaluatedLight(candidates[i].id, *candidates[i].light,
                       scene.getWorldMatrix(candidates[i].id), out_lights[i]);
    scaleEvaluatedLightToWorldUnits(out_lights[i], world_per_metre);
  }
  return count;
}

}  // namespace Blunder
