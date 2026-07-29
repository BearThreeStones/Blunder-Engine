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

namespace {

Mat4 blendSkinMatrixGpuReference(const eastl::vector<Mat4>& joint_palette,
                                 const MeshSkinInfluence& influence) {
  Mat4 result(0.0f);
  float weight_sum = 0.0f;
  for (int slot = 0; slot < 4; ++slot) {
    weight_sum += influence.weights[slot];
  }
  if (weight_sum <= 1e-6f) {
    return Mat4(1.0f);
  }

  for (int slot = 0; slot < 4; ++slot) {
    const float weight = influence.weights[slot];
    if (weight <= 0.0f) {
      continue;
    }
    const int joint_slot = influence.joint_indices[slot];
    if (joint_slot < 0 ||
        joint_slot >= static_cast<int>(joint_palette.size()) ||
        joint_slot >= static_cast<int>(k_max_gpu_skin_joints)) {
      continue;
    }
    result += joint_palette[static_cast<size_t>(joint_slot)] * weight;
  }

  if (weight_sum < 0.999f || weight_sum > 1.001f) {
    result /= weight_sum;
  }
  return result;
}

}  // namespace

void applyGpuReferenceSkinning(const Skeleton& skeleton, const MeshSkinData& skin_data,
                               const eastl::vector<MeshVertex>& bind_vertices,
                               eastl::vector<Vec3>& out_positions) {
  out_positions.clear();
  if (!skin_data.isValid() || bind_vertices.empty() ||
      skin_data.influences.size() != bind_vertices.size()) {
    out_positions.resize(bind_vertices.size());
    for (size_t i = 0; i < bind_vertices.size(); ++i) {
      out_positions[i] = bind_vertices[i].position;
    }
    return;
  }

  eastl::vector<Mat4> joint_palette;
  buildGpuBonePalette(skeleton, skin_data, joint_palette);

  const size_t vertex_count = bind_vertices.size();
  out_positions.resize(vertex_count);
  for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
    const MeshSkinInfluence& influence = skin_data.influences[vertex_index];
    const Mat4 skin_matrix =
        blendSkinMatrixGpuReference(joint_palette, influence);
    const Vec4 skinned_position =
        skin_matrix * Vec4(bind_vertices[vertex_index].position, 1.0f);
    out_positions[vertex_index] = Vec3(skinned_position);
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
