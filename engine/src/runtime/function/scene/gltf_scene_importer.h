#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <glm/mat4x4.hpp>

#include "runtime/core/math/geometry.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/function/scene/mesh_renderer_component.h"

namespace Blunder {

class AssetManager;
class SceneInstance;

/// Imports a glTF scene graph into a SceneInstance (one entity per mesh primitive).
class GltfSceneImporter final {
 public:
  struct ImportResult {
    bool success{false};
    eastl::string error_message;
    size_t mesh_primitive_count{0};
    AABB world_bounds{};
    bool has_world_bounds{false};
  };

  static ImportResult importIntoScene(AssetManager* asset_manager,
                                      const eastl::string& virtual_path,
                                      SceneInstance& scene_instance);

  /// Imports a glTF (or mesh descriptor pointing at glTF) under an existing entity.
  static ImportResult importUnderEntity(AssetManager* asset_manager,
                                      const eastl::string& mesh_or_gltf_path,
                                      SceneInstance& scene_instance,
                                      EntityId parent_entity_id);
};

}  // namespace Blunder
