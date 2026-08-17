#include "runtime/core/object/missing_skeleton_modifier.h"

namespace Blunder {

MissingSkeletonModifier::MissingSkeletonModifier(eastl::string authored_type)
    : m_authored_type(eastl::move(authored_type)) {}

const char* MissingSkeletonModifier::getTypeName() const {
  return m_authored_type.c_str();
}

void MissingSkeletonModifier::apply(Skeleton& /*skeleton*/) {}

void MissingSkeletonModifier::setExtraFields(
    eastl::vector<SkeletonModifierExtraField> fields) {
  m_extra_fields = eastl::move(fields);
}

}  // namespace Blunder
