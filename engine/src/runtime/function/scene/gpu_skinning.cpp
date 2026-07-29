#include "runtime/function/scene/gpu_skinning.h"

namespace Blunder {

void buildGpuBonePalette(const Skeleton& skeleton, const MeshSkinData& skin_data,
                         eastl::vector<Mat4>& out_joint_matrices) {
  out_joint_matrices.clear();
  if (!skin_data.isValid()) {
    return;
  }

  const size_t joint_count = skin_data.joint_to_bone.size();
  out_joint_matrices.resize(joint_count, Mat4(1.0f));
  for (size_t joint_index = 0; joint_index < joint_count; ++joint_index) {
    const int bone_index = skin_data.joint_to_bone[joint_index];
    if (bone_index < 0 ||
        bone_index >= static_cast<int>(skeleton.getBoneCount())) {
      continue;
    }
    out_joint_matrices[joint_index] =
        skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(bone_index)) *
        skeleton.getBoneInverseBind(static_cast<size_t>(bone_index));
  }
}

void packSkinnedMeshVertices(const MeshAsset& mesh_asset,
                             eastl::vector<SkinnedMeshVertex>& out_vertices) {
  const eastl::vector<MeshVertex>& bind_vertices = mesh_asset.getVertices();
  const MeshSkinData& skin_data = mesh_asset.getSkinData();
  if (!skin_data.isValid() || bind_vertices.empty()) {
    out_vertices.clear();
    return;
  }

  const size_t vertex_count = bind_vertices.size();
  out_vertices.resize(vertex_count);
  for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
    const MeshVertex& bind_vertex = bind_vertices[vertex_index];
    const MeshSkinInfluence& influence =
        vertex_index < skin_data.influences.size()
            ? skin_data.influences[vertex_index]
            : MeshSkinInfluence{};

    SkinnedMeshVertex& out_vertex = out_vertices[vertex_index];
    out_vertex.position = bind_vertex.position;
    out_vertex.normal = bind_vertex.normal;
    out_vertex.uv = bind_vertex.uv;
    out_vertex.tangent = bind_vertex.tangent;
    out_vertex.joint_indices = influence.joint_indices;
    out_vertex.weights = influence.weights;
  }
}

}  // namespace Blunder
