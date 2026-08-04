#include "runtime/core/object/skeleton_modifier.h"

#include "runtime/core/object/skeleton.h"

namespace Blunder {

void SkeletonModifier::setApplyFn(SkeletonModifierApplyFn fn, void* userdata) {
  m_apply_fn = fn;
  m_apply_userdata = userdata;
}

void SkeletonModifier::apply(Skeleton& skeleton) const {
  if (!m_enabled || m_apply_fn == nullptr) {
    return;
  }
  m_apply_fn(skeleton, m_apply_userdata);
}

}  // namespace Blunder
