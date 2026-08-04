#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_id.h"
#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

/// Host bone → child Object Transform SkeletonModifier product (Phase 6).
class SkeletonAttachModifier : public SkeletonModifier {
 public:
  void setBoneName(const eastl::string& name) { m_bone_name = name; }
  const eastl::string& getBoneName() const { return m_bone_name; }

  void setChildObjectId(ObjectId child_id) { m_child_object_id = child_id; }
  ObjectId getChildObjectId() const { return m_child_object_id; }

  void apply(Skeleton& skeleton) override;

 private:
  eastl::string m_bone_name{"Hand"};
  ObjectId m_child_object_id{k_invalid_object_id};
};

}  // namespace Blunder
