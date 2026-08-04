#pragma once

namespace Blunder {

class Skeleton;

using SkeletonModifierApplyFn = void (*)(Skeleton& skeleton, void* userdata);
using SkeletonModifierChainFn = void (*)(Skeleton& skeleton, void* userdata);

/// Post-pose procedural step (Phase 5). Runs after Player/Tree sample, before PoseApplied.
class SkeletonModifier {
 public:
  void setEnabled(bool enabled) { m_enabled = enabled; }
  bool isEnabled() const { return m_enabled; }

  void setApplyFn(SkeletonModifierApplyFn fn, void* userdata);
  void apply(Skeleton& skeleton) const;

 private:
  bool m_enabled{true};
  SkeletonModifierApplyFn m_apply_fn{nullptr};
  void* m_apply_userdata{nullptr};
};

}  // namespace Blunder
