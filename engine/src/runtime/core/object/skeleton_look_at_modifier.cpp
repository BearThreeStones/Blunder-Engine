#include "runtime/core/object/skeleton_look_at_modifier.h"

#include <glm/gtc/quaternion.hpp>

#include "runtime/core/object/skeleton.h"

namespace Blunder {
namespace {

Quat quatFromTo(const Vec3& from, const Vec3& to) {
  const Vec3 unit_from = glm::normalize(from);
  const Vec3 unit_to = glm::normalize(to);
  const float dot = glm::clamp(glm::dot(unit_from, unit_to), -1.0f, 1.0f);
  if (dot >= 1.0f - 1e-6f) {
    return glm::identity<Quat>();
  }
  if (dot <= -1.0f + 1e-6f) {
    Vec3 axis = glm::cross(Vec3(1.0f, 0.0f, 0.0f), unit_from);
    if (glm::dot(axis, axis) < 1e-8f) {
      axis = glm::cross(Vec3(0.0f, 0.0f, 1.0f), unit_from);
    }
    axis = glm::normalize(axis);
    return glm::angleAxis(glm::pi<float>(), axis);
  }
  const Vec3 axis = glm::normalize(glm::cross(unit_from, unit_to));
  return glm::angleAxis(std::acos(dot), axis);
}

}  // namespace

void SkeletonLookAtModifier::apply(Skeleton& skeleton) {
  if (!isEnabled()) {
    return;
  }

  const int bone_index = skeleton.findBoneIndex(m_bone_name);
  if (bone_index < 0) {
    return;
  }

  const size_t idx = static_cast<size_t>(bone_index);
  const Mat4 global_mtx = skeleton.getBoneGlobalPoseMatrix(idx);
  const Vec3 bone_world_pos(global_mtx[3]);

  Vec3 to_target = m_target - bone_world_pos;
  if (glm::dot(to_target, to_target) < 1e-8f) {
    return;
  }
  to_target = glm::normalize(to_target);

  Quat parent_world_rot = glm::identity<Quat>();
  const int parent_index = skeleton.getParentIndex(idx);
  if (parent_index >= 0) {
    const Mat4 parent_global =
        skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(parent_index));
    parent_world_rot = glm::quat_cast(parent_global);
  }

  const Vec3 desired_local_dir = glm::inverse(parent_world_rot) * to_target;
  const Vec3 rest_forward = Vec3(0.0f, 1.0f, 0.0f);
  const Quat aim_delta = quatFromTo(rest_forward, desired_local_dir);

  BoneTransform pose = skeleton.getBonePoseLocal(idx);
  pose.rotation = aim_delta * pose.rotation;
  skeleton.setBonePoseLocal(idx, pose);
}

}  // namespace Blunder
