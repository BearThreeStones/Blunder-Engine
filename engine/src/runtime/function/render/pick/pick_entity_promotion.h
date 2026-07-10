#include <cstdint>

#include "EASTL/vector.h"

#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

/// Promotes a leaf pick hit to the scene-root child (entity whose parent has no
/// parent). Walks up while the current entity's parent has a valid grandparent.
inline EntityId promotePickEntity(SceneInstance& scene, EntityId leaf) {
  if (!isValid(leaf)) {
    return k_invalid_entity_id;
  }
  EntityId current = leaf;
  while (true) {
    const Entity* entity = scene.getEntity(current);
    if (entity == nullptr) {
      return current;
    }
    const EntityId parent_id = entity->getParentId();
    if (!isValid(parent_id)) {
      return current;
    }
    const Entity* parent_entity = scene.getEntity(parent_id);
    if (parent_entity == nullptr) {
      return current;
    }
    const EntityId grandparent_id = parent_entity->getParentId();
    if (!isValid(grandparent_id)) {
      return parent_id;
    }
    current = parent_id;
  }
}

inline void promotePickEntityList(SceneInstance& scene,
                                  eastl::vector<EntityId>& entity_ids) {
  for (EntityId& entity_id : entity_ids) {
    entity_id = promotePickEntity(scene, entity_id);
  }
}

inline eastl::vector<EntityId> dedupePromotedPickHits(
    SceneInstance& scene, const eastl::vector<EntityId>& leaf_hits) {
  eastl::vector<EntityId> promoted;
  promoted.reserve(leaf_hits.size());
  for (const EntityId leaf : leaf_hits) {
    const EntityId entity_id = promotePickEntity(scene, leaf);
    if (!isValid(entity_id)) {
      continue;
    }
    bool found = false;
    for (const EntityId existing : promoted) {
      if (existing == entity_id) {
        found = true;
        break;
      }
    }
    if (!found) {
      promoted.push_back(entity_id);
    }
  }
  return promoted;
}

}  // namespace Blunder
