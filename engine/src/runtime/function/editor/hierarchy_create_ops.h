#pragma once

#include "EASTL/string.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class SceneInstance;

enum class HierarchyCreateKind {
  empty,
  camera,
  light,
};

struct HierarchyCreateResult {
  EntityId entity_id{k_invalid_entity_id};
  bool created{false};
};

bool parseHierarchyCreateKind(const eastl::string& name, HierarchyCreateKind& out_kind);

const char* hierarchyCreateKindBaseName(HierarchyCreateKind kind);
eastl::string hierarchyCreateCommandLabel(const eastl::string& entity_name);

eastl::string uniqueHierarchyCreateName(const SceneInstance& scene,
                                        const eastl::string& base);

/// Spawn a new entity. `parent_id` 0 / invalid = Scene Tree root. Null scene is a no-op.
HierarchyCreateResult applyHierarchyCreate(SceneInstance* scene, EntityId parent_id,
                                           HierarchyCreateKind kind);

}  // namespace Blunder
