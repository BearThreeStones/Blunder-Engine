#include "EASTL/sort.h"
#include "runtime/function/render/pick/pick_broad_hits.h"
#include "runtime/function/render/pick/pick_entity_promotion.h"

#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

eastl::vector<BroadHit> sortBroadHitsByDistance(eastl::vector<BroadHit> hits) {
  eastl::sort(hits.begin(), hits.end(),
              [](const BroadHit& a, const BroadHit& b) { return a.t < b.t; });
  return hits;
}

eastl::vector<EntityId> promoteAndDedupeBroadHits(
    SceneInstance& scene, const eastl::vector<BroadHit>& hits) {
  eastl::vector<EntityId> leaf_ids;
  leaf_ids.reserve(hits.size());
  for (const BroadHit& hit : hits) {
    leaf_ids.push_back(hit.entity_id);
  }
  return dedupePromotedPickHits(scene, leaf_ids);
}

}  // namespace Blunder
