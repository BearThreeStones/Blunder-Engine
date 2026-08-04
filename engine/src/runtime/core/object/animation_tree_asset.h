#pragma once

#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AnimationTree;

/// Clear authored graph fields (keeps player/skeleton/modifier bindings).
void clearAnimationTreeTopology(AnimationTree& tree);

/// Apply Asset/embed topology (points, states, base nodes, Add2/OneShot slots).
/// Does not set active or travel unless `topology` carries those via overrides.
bool applyAnimationTreeTopologyData(AnimationTree& tree,
                                    const AnimationTreeTopologyData& topology);

void captureAnimationTreeTopologyData(const AnimationTree& tree,
                                      AnimationTreeTopologyData& out_topology);

/// Allowlisted instance overrides on top of Asset base.
void applyAnimationTreeInstanceOverrides(
    AnimationTree& tree, const AnimationTreeInstanceOverrides& overrides);

}  // namespace Blunder
