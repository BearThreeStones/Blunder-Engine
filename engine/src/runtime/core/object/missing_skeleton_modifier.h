#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/object/skeleton_modifier.h"
#include "runtime/core/object/skeleton_modifier_extra_field.h"

namespace Blunder {

class Skeleton;

/// Chain slot for a type name absent from the SkeletonModifier type catalog.
class MissingSkeletonModifier final : public SkeletonModifier {
 public:
  explicit MissingSkeletonModifier(eastl::string authored_type);

  const char* getTypeName() const override;
  bool isMissing() const override { return true; }
  void apply(Skeleton& skeleton) override;

  void setExtraFields(eastl::vector<SkeletonModifierExtraField> fields);
  const eastl::vector<SkeletonModifierExtraField>& extraFields() const {
    return m_extra_fields;
  }

 private:
  eastl::string m_authored_type;
  eastl::vector<SkeletonModifierExtraField> m_extra_fields;
};

}  // namespace Blunder
