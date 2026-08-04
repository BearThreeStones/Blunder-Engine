#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

/// Configurable LookAt/aim SkeletonModifier product (Phase 6).
class SkeletonLookAtModifier : public SkeletonModifier {
 public:
  void setBoneName(const eastl::string& name) { m_bone_name = name; }
  const eastl::string& getBoneName() const { return m_bone_name; }

  void setTarget(const Vec3& target) { m_target = target; }
  const Vec3& getTarget() const { return m_target; }

  void apply(Skeleton& skeleton) override;

 private:
  eastl::string m_bone_name{"Head"};
  Vec3 m_target{0.0f, 0.0f, 1.0f};
};

}  // namespace Blunder
