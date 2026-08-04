#pragma once

#include "runtime/core/object/object_id.h"
#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

/// Dispatch method keys crossed while advancing the dominant-clip clock forward.
/// Uses MessageDispatch to co-located Behaviours (not PtrCall).
void dispatchAnimationMethodKeysCrossed(ObjectId target,
                                        const AnimationClipData& clip,
                                        float prev_time, float new_time,
                                        bool looping);

}  // namespace Blunder
