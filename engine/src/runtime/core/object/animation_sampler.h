#pragma once

#include "EASTL/hash_set.h"
#include "EASTL/string.h"

#include "runtime/core/object/skeleton.h"
#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

/// Allow-list for Blend2 and Add2. Null or disabled passes every bone.
/// Enabled with a missing name keeps that bone out of the blend.
struct AnimationBoneFilter {
  bool enabled{false};
  const eastl::hash_set<eastl::string>* bones{nullptr};
};

inline bool animationBonePassesFilter(const eastl::string& bone_name,
                                      const AnimationBoneFilter* filter) {
  if (filter == nullptr || !filter->enabled) {
    return true;
  }
  if (filter->bones == nullptr) {
    return false;
  }
  return filter->bones->find(bone_name) != filter->bones->end();
}

/// Samples clip tracks at `time` onto matching bones in `skeleton`.
/// Bones without tracks keep rest pose. Unknown bone names are ignored.
void sampleClipOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip,
                            float time);

/// Combines two sampled clips onto `skeleton` using local TRS blend.
/// `blend_weight` 0 = entirely clip0, 1 = entirely clip1.
/// When `bone_filter` is enabled, bones outside the allow-list stay on clip0.
void blendClipsOntoSkeleton(Skeleton& skeleton, const AnimationClipData& clip0,
                            float time0, const AnimationClipData& clip1,
                            float time1, float blend_weight,
                            const AnimationBoneFilter* bone_filter = nullptr);

/// Combines three sampled clips with barycentric weights (expected to sum ≈ 1).
/// Translation/scale: weighted lerp. Rotation: signed nlerp then normalize.
void blendThreeClipsOntoSkeleton(Skeleton& skeleton,
                                 const AnimationClipData& clip0, float time0,
                                 float weight0, const AnimationClipData& clip1,
                                 float time1, float weight1,
                                 const AnimationClipData& clip2, float time2,
                                 float weight2);

/// Applies clip tracks as bind/rest-relative additive deltas onto the current pose.
/// Translation/scale: pose += weight * (sampled - rest). Rotation: pose *= slerp(id, sampled * inverse(rest), weight).
/// When `bone_filter` is enabled, bones outside the allow-list are left unchanged.
void applyAdditiveClipOntoSkeleton(
    Skeleton& skeleton, const AnimationClipData& clip, float time, float weight,
    const AnimationBoneFilter* bone_filter = nullptr);

}  // namespace Blunder
