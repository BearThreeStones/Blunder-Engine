#include "runtime/core/object/skeleton.h"

namespace Blunder {

int Skeleton::addBone(eastl::string name, int parent_index) {
  if (parent_index >= static_cast<int>(m_bones.size())) {
    return -1;
  }
  Bone bone;
  bone.name = eastl::move(name);
  bone.parent_index = parent_index;
  bone.pose_local = bone.rest_local;
  m_bones.push_back(eastl::move(bone));
  return static_cast<int>(m_bones.size()) - 1;
}

int Skeleton::findBoneIndex(const eastl::string& name) const {
  for (size_t i = 0; i < m_bones.size(); ++i) {
    if (m_bones[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

const eastl::string& Skeleton::getBoneName(size_t index) const {
  static const eastl::string k_empty;
  if (index >= m_bones.size()) {
    return k_empty;
  }
  return m_bones[index].name;
}

int Skeleton::getParentIndex(size_t index) const {
  if (index >= m_bones.size()) {
    return -1;
  }
  return m_bones[index].parent_index;
}

void Skeleton::setBoneRestLocal(size_t index, const BoneTransform& transform) {
  if (index >= m_bones.size()) {
    return;
  }
  m_bones[index].rest_local = transform;
}

BoneTransform Skeleton::getBoneRestLocal(size_t index) const {
  if (index >= m_bones.size()) {
    return {};
  }
  return m_bones[index].rest_local;
}

void Skeleton::setBonePoseLocal(size_t index, const BoneTransform& transform) {
  if (index >= m_bones.size()) {
    return;
  }
  m_bones[index].pose_local = transform;
}

BoneTransform Skeleton::getBonePoseLocal(size_t index) const {
  if (index >= m_bones.size()) {
    return {};
  }
  return m_bones[index].pose_local;
}

void Skeleton::setBoneInverseBind(size_t index, const Mat4& matrix) {
  if (index >= m_bones.size()) {
    return;
  }
  m_bones[index].inverse_bind = matrix;
}

Mat4 Skeleton::getBoneInverseBind(size_t index) const {
  if (index >= m_bones.size()) {
    return Mat4(1.0f);
  }
  return m_bones[index].inverse_bind;
}

Mat4 Skeleton::getBoneGlobalRestMatrix(size_t index) const {
  return computeGlobalMatrix(index, false);
}

Mat4 Skeleton::getBoneGlobalPoseMatrix(size_t index) const {
  return computeGlobalMatrix(index, true);
}

void Skeleton::resetPoseToRest() {
  for (Bone& bone : m_bones) {
    bone.pose_local = bone.rest_local;
  }
}

Mat4 Skeleton::boneTransformToMatrix(const BoneTransform& transform) {
  const Mat4 translation = glm::translate(Mat4(1.0f), transform.translation);
  const Mat4 rotation = glm::mat4_cast(transform.rotation);
  const Mat4 scale = glm::scale(Mat4(1.0f), transform.scale);
  return translation * rotation * scale;
}

Mat4 Skeleton::computeGlobalMatrix(size_t index, bool use_pose) const {
  if (index >= m_bones.size()) {
    return Mat4(1.0f);
  }
  const Bone& bone = m_bones[index];
  const Mat4 local =
      boneTransformToMatrix(use_pose ? bone.pose_local : bone.rest_local);
  if (bone.parent_index >= 0) {
    return computeGlobalMatrix(static_cast<size_t>(bone.parent_index),
                               use_pose) *
           local;
  }
  return local;
}

}  // namespace Blunder
