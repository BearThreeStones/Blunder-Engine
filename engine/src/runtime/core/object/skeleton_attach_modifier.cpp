#include "runtime/core/object/skeleton_attach_modifier.h"

#include <glm/gtc/quaternion.hpp>

#include "runtime/core/object/object_db.h"
#include "runtime/core/object/skeleton.h"

namespace Blunder {
namespace {

void decomposeMatrixTRS(const Mat4& matrix, Vec3& position, Quat& rotation,
                        Vec3& scale) {
  position = Vec3(matrix[3]);
  Vec3 col0(matrix[0]);
  Vec3 col1(matrix[1]);
  Vec3 col2(matrix[2]);
  scale.x = glm::length(col0);
  scale.y = glm::length(col1);
  scale.z = glm::length(col2);
  if (scale.x > 1e-8f) {
    col0 /= scale.x;
  }
  if (scale.y > 1e-8f) {
    col1 /= scale.y;
  }
  if (scale.z > 1e-8f) {
    col2 /= scale.z;
  }
  const Mat3 rot_mat(col0, col1, col2);
  rotation = glm::quat_cast(Mat4(rot_mat));
}

}  // namespace

void SkeletonAttachModifier::apply(Skeleton& skeleton) {
  if (!isEnabled()) {
    return;
  }
  if (!isValid(m_child_object_id)) {
    m_last_apply_status = SkeletonAttachApplyStatus::SkippedInvalidChild;
    return;
  }

  const int bone_index = skeleton.findBoneIndex(m_bone_name);
  if (bone_index < 0) {
    m_last_apply_status = SkeletonAttachApplyStatus::SkippedInvalidBone;
    return;
  }

  Object* child = ObjectDB::get(m_child_object_id);
  if (child == nullptr) {
    m_last_apply_status = SkeletonAttachApplyStatus::SkippedChildNotFound;
    return;
  }

  const Mat4 bone_global =
      skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(bone_index));
  Vec3 position;
  Quat rotation;
  Vec3 scale;
  decomposeMatrixTRS(bone_global, position, rotation, scale);

  child->setPosition(position);
  child->setRotation(rotation);
  child->setScale(scale);
  m_last_apply_status = SkeletonAttachApplyStatus::Applied;
}

}  // namespace Blunder
