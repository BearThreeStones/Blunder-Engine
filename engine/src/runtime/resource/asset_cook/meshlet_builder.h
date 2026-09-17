#pragma once

#include "EASTL/vector.h"

#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/meshlet.h"

namespace Blunder {

/// Cook-time Meshlets for static meshes. Empty when skinned, degenerate, or
/// MeshOptimizer cannot emit a cluster.
MeshletPayload buildStaticMeshlets(const eastl::vector<MeshVertex>& vertices,
                                   const eastl::vector<uint32_t>& indices);

}  // namespace Blunder
