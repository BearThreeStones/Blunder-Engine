#pragma once

#include "EASTL/shared_ptr.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

namespace Blunder {

class AssetManager;
class MaterialAsset;
class MeshAsset;

/// One drawable primitive for Mesh Preview (submesh + material).
struct MeshPreviewSubmeshDraw {
  eastl::shared_ptr<MeshAsset> mesh;
  eastl::shared_ptr<MaterialAsset> material;
};

/// Collect all glTF primitives for a Mesh descriptor or glTF path. Cooked Final
/// assets return a single entry; Intermediate/Fast Path enumerates every primitive.
eastl::vector<MeshPreviewSubmeshDraw> collectMeshPreviewSubmeshes(
    AssetManager& asset_manager, const eastl::string& mesh_virtual_path);

}  // namespace Blunder
