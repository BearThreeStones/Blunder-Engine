#pragma once

namespace Blunder {

class Skeleton;

using SkeletonModifierApplyFn = void (*)(Skeleton& skeleton, void* userdata);
using SkeletonModifierChainFn = void (*)(Skeleton& skeleton, void* userdata);

/// Post-pose procedural step (Phase 5). Runs after Player/Tree sample, before PoseApplied.
class SkeletonModifier {
 public:
  virtual ~SkeletonModifier() = default;

  void setEnabled(bool enabled) { m_enabled = enabled; }
  bool isEnabled() const { return m_enabled; }

  void setApplyFn(SkeletonModifierApplyFn fn, void* userdata);

  /// ClassDB name of the concrete product; drives scene serialization.
  virtual const char* getTypeName() const { return "SkeletonModifier"; }

  /// Extension point: subclasses override; default delegates to setApplyFn.
  virtual void apply(Skeleton& skeleton);

 protected:
  SkeletonModifierApplyFn m_apply_fn{nullptr};
  void* m_apply_userdata{nullptr};
  bool m_enabled{true};
};

}  // namespace Blunder
