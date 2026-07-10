#pragma once

#include "EASTL/vector.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;

struct BroadHit {
  EntityId entity_id{k_invalid_entity_id};
  float t{0.0f};
};

eastl::vector<BroadHit> sortBroadHitsByDistance(eastl::vector<BroadHit> hits);

eastl::vector<EntityId> promoteAndDedupeBroadHits(SceneInstance& scene,
                                                  const eastl::vector<BroadHit>& hits);

}  // namespace Blunder
