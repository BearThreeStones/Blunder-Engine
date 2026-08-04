#pragma once

#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

/// Test double proving the SkeletonModifier extension point (Phase 5 Gate A).
class SkeletonModifierTestDouble : public SkeletonModifier {
 public:
  int getApplyCount() const { return m_apply_count; }
  int getRecordedOrder() const { return m_recorded_order; }

  void setOrderCounter(int* counter) { m_order_counter = counter; }
  void setBoneXOffset(float offset) { m_bone_x_offset = offset; }

  void resetSpy();

  void apply(Skeleton& skeleton) override;

 private:
  int m_apply_count{0};
  int m_recorded_order{0};
  int* m_order_counter{nullptr};
  float m_bone_x_offset{0.0f};
};

}  // namespace Blunder
