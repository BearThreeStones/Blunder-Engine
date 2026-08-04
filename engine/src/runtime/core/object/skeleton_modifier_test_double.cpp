#include "runtime/core/object/skeleton_modifier_test_double.h"

#include "runtime/core/object/skeleton.h"

namespace Blunder {

void SkeletonModifierTestDouble::resetSpy() {
  m_apply_count = 0;
  m_recorded_order = 0;
}

void SkeletonModifierTestDouble::apply(Skeleton& skeleton) {
  if (!isEnabled()) {
    return;
  }
  ++m_apply_count;
  if (m_order_counter != nullptr) {
    m_recorded_order = ++(*m_order_counter);
  }
  if (m_bone_x_offset != 0.0f && skeleton.getBoneCount() > 0) {
    BoneTransform pose = skeleton.getBonePoseLocal(0);
    pose.translation.x += m_bone_x_offset;
    skeleton.setBonePoseLocal(0, pose);
  }
}

}  // namespace Blunder
