#pragma once

#include "EASTL/vector.h"

#include "runtime/core/object/skeleton.h"
#include "runtime/resource/asset/mesh_asset.h"
#include "runtime/resource/asset/mesh_skin_data.h"

namespace Blunder {

/// Max joint matrices in the GPU skinning UBO (128 × 64 B = 8 KiB std140).
constexpr uint32_t k_max_gpu_skin_joints = 128;

/// Cooked Final with skin payload → GPU skinning; Intermediate-only → CPU Fast Path.
inline bool shouldUseGpuSkinning(const MeshAsset& mesh_asset) {
  return mesh_asset.hasCookedFinalSkin();
}

/// Joint palette: global_pose * inverse_bind per glTF joint slot (matches CPU path).
void buildGpuBonePalette(const Skeleton& skeleton, const MeshSkinData& skin_data,
                         eastl::vector<Mat4>& out_joint_matrices);

/// Pack bind-pose vertices with per-vertex joint indices/weights for GPU skinning.
void packSkinnedMeshVertices(const MeshAsset& mesh_asset,
                             eastl::vector<SkinnedMeshVertex>& out_vertices);

/// CPU mirror of `blendSkinMatrix` + position multiply in pbr_skinned.slang (positions only).
void applyGpuReferenceSkinning(const Skeleton& skeleton, const MeshSkinData& skin_data,
                               const eastl::vector<MeshVertex>& bind_vertices,
                               eastl::vector<Vec3>& out_positions);

}  // namespace Blunder
