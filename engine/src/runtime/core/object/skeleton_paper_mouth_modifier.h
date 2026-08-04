#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

/// PaperMouth jaw-open SkeletonModifier product (Phase 6).
class SkeletonPaperMouthModifier : public SkeletonModifier {
 public:
  void setOpenAmount(float amount) { m_open_amount = amount; }
  float getOpenAmount() const { return m_open_amount; }

  void setBoneName(const eastl::string& name) { m_bone_name = name; }
  const eastl::string& getBoneName() const { return m_bone_name; }

  void apply(Skeleton& skeleton) override;

 private:
  eastl::string m_bone_name{"Jaw"};
  float m_open_amount{0.0f};
};

}  // namespace Blunder
