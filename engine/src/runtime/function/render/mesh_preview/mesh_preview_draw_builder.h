#pragma once

#include <glm/mat4x4.hpp>

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/scene/entity_id.h"

namespace Blunder {

class AssetManager;
class MaterialAsset;
class MeshAsset;

/// One drawable primitive for Mesh Preview (submesh + material + node Xform).
struct MeshPreviewSubmeshDraw {
  eastl::shared_ptr<MeshAsset> mesh;
  eastl::shared_ptr<MaterialAsset> material;
  /// Engine-space node / entity world matrix (identity when unavailable).
  glm::mat4 model{1.0f};
  EntityId entity_id{k_invalid_entity_id};
};

/// Collect all glTF primitives for a Mesh descriptor or glTF path. Cooked Final
/// assets return a single entry; Intermediate/Fast Path enumerates every primitive.
eastl::vector<MeshPreviewSubmeshDraw> collectMeshPreviewSubmeshes(
    AssetManager& asset_manager, const eastl::string& mesh_virtual_path);

}  // namespace Blunder
