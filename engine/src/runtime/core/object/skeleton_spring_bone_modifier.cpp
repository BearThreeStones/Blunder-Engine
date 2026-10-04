#include "runtime/core/object/skeleton_spring_bone_modifier.h"

#include <cmath>

#include <glm/common.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/core/object/skeleton.h"

namespace Blunder {
namespace {

constexpr float k_spring_max_dt = 0.05f;
constexpr float k_spring_length_epsilon = 1.0e-4f;

Quat quatFromTo(const Vec3& from, const Vec3& to) {
  const float from_len2 = glm::dot(from, from);
  const float to_len2 = glm::dot(to, to);
  if (from_len2 < 1.0e-12f || to_len2 < 1.0e-12f) {
    return glm::identity<Quat>();
  }
  const Vec3 unit_from = from * (1.0f / std::sqrt(from_len2));
  const Vec3 unit_to = to * (1.0f / std::sqrt(to_len2));
  const float dot = glm::clamp(glm::dot(unit_from, unit_to), -1.0f, 1.0f);
  if (dot >= 1.0f - 1.0e-6f) {
    return glm::identity<Quat>();
  }
  if (dot <= -1.0f + 1.0e-6f) {
    Vec3 axis = glm::cross(Vec3(1.0f, 0.0f, 0.0f), unit_from);
    if (glm::dot(axis, axis) < 1.0e-8f) {
      axis = glm::cross(Vec3(0.0f, 0.0f, 1.0f), unit_from);
    }
    axis = glm::normalize(axis);
    return glm::angleAxis(glm::pi<float>(), axis);
  }
  const Vec3 axis = glm::normalize(glm::cross(unit_from, unit_to));
  return glm::angleAxis(std::acos(dot), axis);
}

Vec3 translationOf(const Mat4& matrix) { return Vec3(matrix[3]); }

bool sameChain(const eastl::vector<int>& a, const eastl::vector<int>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

bool buildChain(const Skeleton& skeleton, const eastl::string& root_name,
                const eastl::string& end_name, eastl::vector<int>& out) {
  out.clear();
  const int root = skeleton.findBoneIndex(root_name);
  if (root < 0) {
    return false;
  }
  if (end_name.empty() || end_name == root_name) {
    out.push_back(root);
    return true;
  }
  const int end = skeleton.findBoneIndex(end_name);
  if (end < 0) {
    return false;
  }

  eastl::vector<int> reversed;
  int cursor = end;
  bool found_root = false;
  const int guard = static_cast<int>(skeleton.getBoneCount()) + 1;
  for (int step = 0; step < guard && cursor >= 0; ++step) {
    reversed.push_back(cursor);
    if (cursor == root) {
      found_root = true;
      break;
    }
    cursor = skeleton.getParentIndex(static_cast<size_t>(cursor));
  }
  if (!found_root) {
    return false;
  }
  out.resize(reversed.size());
  for (size_t i = 0; i < reversed.size(); ++i) {
    out[i] = reversed[reversed.size() - 1 - i];
  }
  return true;
}

void aimBone(Skeleton& skeleton, size_t bone_index, const Vec3& local_axis,
             const Vec3& target_model) {
  const Mat4 global_mtx = skeleton.getBoneGlobalPoseMatrix(bone_index);
  const Vec3 head = translationOf(global_mtx);
  Vec3 to_target = target_model - head;
  if (glm::dot(to_target, to_target) < 1.0e-12f) {
    return;
  }
  to_target = glm::normalize(to_target);

  Quat parent_model_rot = glm::identity<Quat>();
  const int parent_index = skeleton.getParentIndex(bone_index);
  if (parent_index >= 0) {
    const Mat4 parent_global =
        skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(parent_index));
    parent_model_rot = glm::quat_cast(parent_global);
  }

  const Vec3 desired_local_dir = glm::inverse(parent_model_rot) * to_target;
  BoneTransform pose = skeleton.getBonePoseLocal(bone_index);
  const Quat aim_delta =
      quatFromTo(pose.rotation * local_axis, desired_local_dir);
  pose.rotation = aim_delta * pose.rotation;
  skeleton.setBonePoseLocal(bone_index, pose);
}

}  // namespace

void SkeletonSpringBoneModifier::setRootBoneName(const eastl::string& name) {
  if (m_root_bone_name == name) {
    return;
  }
  m_root_bone_name = name;
  resetSimulation();
}

void SkeletonSpringBoneModifier::setEndBoneName(const eastl::string& name) {
  if (m_end_bone_name == name) {
    return;
  }
  m_end_bone_name = name;
  resetSimulation();
}

void SkeletonSpringBoneModifier::setStiffness(float stiffness) {
  m_stiffness = glm::clamp(stiffness, 0.0f, 1.0f);
}

void SkeletonSpringBoneModifier::setDrag(float drag) {
  m_drag = glm::clamp(drag, 0.0f, 1.0f);
}

void SkeletonSpringBoneModifier::setEndBoneLength(float length) {
  m_end_bone_length =
      length > k_spring_length_epsilon ? length : k_spring_length_epsilon;
}

void SkeletonSpringBoneModifier::resetSimulation() {
  m_initialized = false;
  m_last_dt = 0.0f;
  m_chain.clear();
  m_tail.clear();
  m_prev_tail.clear();
}

void SkeletonSpringBoneModifier::apply(Skeleton& skeleton) {
  if (!isEnabled()) {
    resetSimulation();
    return;
  }

  eastl::vector<int> chain;
  if (!buildChain(skeleton, m_root_bone_name, m_end_bone_name, chain)) {
    return;
  }
  if (!sameChain(chain, m_chain)) {
    resetSimulation();
    m_chain = chain;
  }

  const size_t count = m_chain.size();
  eastl::vector<Vec3> local_axis(count, Vec3(0.0f, 1.0f, 0.0f));
  eastl::vector<float> length(count, m_end_bone_length);
  eastl::vector<Quat> animated_local(count, glm::identity<Quat>());

  for (size_t i = 0; i < count; ++i) {
    const size_t bone_index = static_cast<size_t>(m_chain[i]);
    animated_local[i] = skeleton.getBonePoseLocal(bone_index).rotation;
    if (i + 1 < count) {
      const Vec3 offset = skeleton
                              .getBonePoseLocal(static_cast<size_t>(m_chain[i + 1]))
                              .translation;
      const float offset_len = glm::length(offset);
      if (offset_len > k_spring_length_epsilon) {
        local_axis[i] = offset / offset_len;
        length[i] = offset_len;
      }
    }
  }

  const Mat4 host_inv = glm::inverse(m_host_world);
  auto model_to_world = [this](const Vec3& model) {
    return Vec3(m_host_world * Vec4(model, 1.0f));
  };
  auto vector_to_world = [this](const Vec3& vector) {
    return Vec3(m_host_world * Vec4(vector, 0.0f));
  };
  auto world_to_model = [&host_inv](const Vec3& world) {
    return Vec3(host_inv * Vec4(world, 1.0f));
  };

  auto joint_goal_world = [&](size_t i) {
    const size_t bone_index = static_cast<size_t>(m_chain[i]);
    const Vec3 head_model =
        translationOf(skeleton.getBoneGlobalPoseMatrix(bone_index));
    Quat parent_rot = glm::identity<Quat>();
    const int parent_index = skeleton.getParentIndex(bone_index);
    if (parent_index >= 0) {
      parent_rot = glm::quat_cast(
          skeleton.getBoneGlobalPoseMatrix(static_cast<size_t>(parent_index)));
    }
    Vec3 goal_dir_model = parent_rot * (animated_local[i] * local_axis[i]);
    const float dir_len2 = glm::dot(goal_dir_model, goal_dir_model);
    if (dir_len2 > 1.0e-12f) {
      goal_dir_model *= 1.0f / std::sqrt(dir_len2);
    } else {
      goal_dir_model = local_axis[i];
    }
    const Vec3 head_world = model_to_world(head_model);
    return head_world + vector_to_world(goal_dir_model * length[i]);
  };

  if (!m_initialized || m_tail.size() != count) {
    m_tail.resize(count);
    m_prev_tail.resize(count);
    for (size_t i = 0; i < count; ++i) {
      m_tail[i] = joint_goal_world(i);
      m_prev_tail[i] = m_tail[i];
    }
    m_initialized = true;
    m_last_dt = 0.0f;
    return;
  }

  float dt = m_delta_time;
  if (dt < 0.0f) {
    dt = 0.0f;
  }
  if (dt > k_spring_max_dt) {
    dt = k_spring_max_dt;
  }

  if (dt <= 0.0f) {
    for (size_t i = 0; i < count; ++i) {
      aimBone(skeleton, static_cast<size_t>(m_chain[i]), local_axis[i],
              world_to_model(m_tail[i]));
    }
    return;
  }

  const float prev_dt = m_last_dt > 1.0e-5f ? m_last_dt : dt;
  const float keep = 1.0f - m_drag;
  for (size_t i = 0; i < count; ++i) {
    const Vec3 goal = joint_goal_world(i);
    const size_t bone_index = static_cast<size_t>(m_chain[i]);
    const Vec3 head_world = model_to_world(
        translationOf(skeleton.getBoneGlobalPoseMatrix(bone_index)));
    const Vec3 velocity = (m_tail[i] - m_prev_tail[i]) / prev_dt;
    Vec3 next = m_tail[i] + velocity * dt * keep + m_gravity * dt * dt;
    next = glm::mix(next, goal, m_stiffness);
    Vec3 rest_offset = goal - head_world;
    float world_length = glm::length(rest_offset);
    if (world_length < 1.0e-8f) {
      rest_offset = vector_to_world(local_axis[i] * length[i]);
      world_length = glm::length(rest_offset);
    }
    Vec3 offset = next - head_world;
    if (glm::dot(offset, offset) < 1.0e-12f) {
      offset = rest_offset;
    }
    if (world_length < 1.0e-8f || glm::dot(offset, offset) < 1.0e-12f) {
      next = head_world;
    } else {
      next = head_world + glm::normalize(offset) * world_length;
    }
    m_prev_tail[i] = m_tail[i];
    m_tail[i] = next;
    aimBone(skeleton, bone_index, local_axis[i], world_to_model(next));
  }
  m_last_dt = dt;
}

}  // namespace Blunder
