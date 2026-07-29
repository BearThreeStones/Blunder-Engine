#pragma once

#include "runtime/core/object/skeleton.h"
#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

/// Samples clip tracks at `time` onto matching bones in `skeleton`.
/// Bones without tracks keep rest pose. Unknown bone names are ignored.
void sampleClipOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip,
                            float time);

}  // namespace Blunder
