#pragma once

#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

class Skeleton;

/// Animation Pipeline stages 4–6 after Local Pose is already on `skeleton`:
/// SkeletonModifier chain → Global Pose recompute → Matrix Palette → done.
/// Callers fire PoseApplied after this returns.
void animationPipelineFinalize(Skeleton& skeleton,
                               SkeletonModifierChainFn modifier_chain,
                               void* modifier_userdata);

/// Stages 5–6 only (no modifiers). Used when Local Pose is final.
void animationPipelineRebuildOutputs(Skeleton& skeleton);

}  // namespace Blunder
