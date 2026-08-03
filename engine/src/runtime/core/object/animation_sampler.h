#pragma once

#include "runtime/core/object/skeleton.h"
#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

/// Samples clip tracks at `time` onto matching bones in `skeleton`.
/// Bones without tracks keep rest pose. Unknown bone names are ignored.
void sampleClipOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip,
                            float time);

/// Combines two sampled clips onto `skeleton` using local TRS blend.
/// `blend_weight` 0 = entirely clip0, 1 = entirely clip1.
void blendClipsOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip0,
                            float time0, const AnimationClipData& clip1,
                            float time1, float blend_weight);

/// Applies clip tracks as bind/rest-relative additive deltas onto the current pose.
/// Translation/scale: pose += weight * (sampled - rest). Rotation: pose *= slerp(id, sampled * inverse(rest), weight).
void applyAdditiveClipOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip,
                                   float time, float weight);

}  // namespace Blunder
