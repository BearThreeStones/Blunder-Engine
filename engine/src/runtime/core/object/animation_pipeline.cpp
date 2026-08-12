#include "runtime/core/object/animation_pipeline.h"

#include "runtime/core/object/skeleton.h"

namespace Blunder {

void animationPipelineRebuildOutputs(Skeleton& skeleton) {
  skeleton.rebuildPoseBuffers();
}

void animationPipelineFinalize(Skeleton& skeleton,
                               SkeletonModifierChainFn modifier_chain,
                               void* modifier_userdata) {
  if (modifier_chain != nullptr) {
    modifier_chain(skeleton, modifier_userdata);
  }
  // Conservative stage 5 + stage 6 after every modifier chain.
  animationPipelineRebuildOutputs(skeleton);
}

}  // namespace Blunder
