#pragma once

#include "EASTL/vector.h"

#include "runtime/core/object/skeleton.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/mesh_skin_data.h"

namespace Blunder {

/// Deform bind-pose vertices with the current Skeleton pose (CPU linear blend skinning).
void applyCpuSkinning(const Skeleton& skeleton, const MeshSkinData& skin_data,
                      const eastl::vector<MeshVertex>& bind_vertices,
                      eastl::vector<MeshVertex>& out_vertices);

}  // namespace Blunder
