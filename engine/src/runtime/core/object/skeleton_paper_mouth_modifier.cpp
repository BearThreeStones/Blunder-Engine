#include "runtime/core/object/skeleton_paper_mouth_modifier.h"

#include <glm/gtc/quaternion.hpp>

#include "runtime/core/object/skeleton.h"

namespace Blunder {
namespace {

const float kMaxJawOpenRadians = 0.785398163f;  // 45 degrees

}  // namespace

void SkeletonPaperMouthModifier::setAttachDriven(bool driven) {
  m_attach_driven = driven;
  if (m_attach_driven) {
    m_open_amount = m_attach_occupancy;
  }
}

void SkeletonPaperMouthModifier::setAttachOccupancy(float occupancy) {
  m_attach_occupancy = occupancy;
  if (m_attach_driven) {
    m_open_amount = occupancy;
  }
}

void SkeletonPaperMouthModifier::apply(Skeleton& skeleton) {
  if (!isEnabled()) {
    return;
  }

  const float clamped_open = glm::clamp(m_open_amount, 0.0f, 1.0f);
  if (clamped_open <= 0.0f) {
    return;
  }

  const int bone_index = skeleton.findBoneIndex(m_bone_name);
  if (bone_index < 0) {
    return;
  }

  const size_t idx = static_cast<size_t>(bone_index);
  const Quat jaw_open =
      glm::angleAxis(clamped_open * kMaxJawOpenRadians, Vec3(1.0f, 0.0f, 0.0f));

  BoneTransform pose = skeleton.getBonePoseLocal(idx);
  pose.rotation = jaw_open * pose.rotation;
  skeleton.setBonePoseLocal(idx, pose);
}

}  // namespace Blunder
