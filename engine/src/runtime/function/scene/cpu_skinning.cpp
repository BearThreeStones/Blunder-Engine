#include "runtime/function/scene/cpu_skinning.h"

#include <glm/gtc/matrix_inverse.hpp>

namespace Blunder {

void applyCpuSkinning(const Skeleton& skeleton, const MeshSkinData& skin_data,
                      const eastl::vector<MeshVertex>& bind_vertices,
                      eastl::vector<MeshVertex>& out_vertices) {
  if (!skin_data.isValid() || bind_vertices.empty()) {
    out_vertices = bind_vertices;
    return;
  }

  const size_t vertex_count = bind_vertices.size();
  if (skin_data.influences.size() != vertex_count) {
    out_vertices = bind_vertices;
    return;
  }

  eastl::vector<Mat4> joint_matrices(skin_data.joint_to_bone.size(), Mat4(1.0f));
  const bool use_pipeline_palette = skeleton.hasValidPoseBuffers();
  for (size_t joint_index = 0; joint_index < skin_data.joint_to_bone.size();
       ++joint_index) {
    const int bone_index = skin_data.joint_to_bone[joint_index];
    if (bone_index < 0 ||
        bone_index >= static_cast<int>(skeleton.getBoneCount())) {
      continue;
    }
    if (use_pipeline_palette) {
      joint_matrices[joint_index] =
          skeleton.getBoneSkinMatrix(static_cast<size_t>(bone_index));
    } else {
      joint_matrices[joint_index] =
          skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(bone_index)) *
          skeleton.getBoneInverseBind(static_cast<size_t>(bone_index));
    }
  }

  out_vertices.resize(vertex_count);
  for (size_t vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
    const MeshVertex& bind_vertex = bind_vertices[vertex_index];
    const MeshSkinInfluence& influence = skin_data.influences[vertex_index];

    Mat4 blended_matrix(0.0f);
    float weight_sum = 0.0f;
    for (int slot = 0; slot < 4; ++slot) {
      const float weight = influence.weights[slot];
      if (weight <= 0.0f) {
        continue;
      }
      const int joint_slot = influence.joint_indices[slot];
      if (joint_slot < 0 ||
          joint_slot >= static_cast<int>(joint_matrices.size())) {
        continue;
      }
      blended_matrix += joint_matrices[joint_slot] * weight;
      weight_sum += weight;
    }

    MeshVertex& out_vertex = out_vertices[vertex_index];
    out_vertex.uv = bind_vertex.uv;
    out_vertex.tangent = bind_vertex.tangent;

    if (weight_sum <= 1e-6f) {
      out_vertex.position = bind_vertex.position;
      out_vertex.normal = bind_vertex.normal;
      continue;
    }

    if (weight_sum < 0.999f || weight_sum > 1.001f) {
      blended_matrix /= weight_sum;
    }

    const Vec4 skinned_position =
        blended_matrix * Vec4(bind_vertex.position, 1.0f);
    out_vertex.position = Vec3(skinned_position);

    const Mat3 normal_matrix =
        glm::transpose(glm::inverse(Mat3(blended_matrix)));
    const Vec3 skinned_normal = normal_matrix * bind_vertex.normal;
    const float normal_length = glm::length(skinned_normal);
    if (normal_length > 1e-6f) {
      out_vertex.normal = skinned_normal / normal_length;
    } else {
      out_vertex.normal = bind_vertex.normal;
    }
  }
}

}  // namespace Blunder
