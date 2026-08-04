#include "runtime/core/object/animation_tree_asset.h"

#include "runtime/core/object/animation_tree.h"

namespace Blunder {

void clearAnimationTreeTopology(AnimationTree& tree) {
  tree.clearAuthoredTopology();
}

bool applyAnimationTreeTopologyData(AnimationTree& tree,
                                    const AnimationTreeTopologyData& topology) {
  return tree.applyTopologyData(topology);
}

void captureAnimationTreeTopologyData(const AnimationTree& tree,
                                      AnimationTreeTopologyData& out_topology) {
  tree.exportTopologyData(out_topology);
}

void applyAnimationTreeInstanceOverrides(
    AnimationTree& tree, const AnimationTreeInstanceOverrides& overrides) {
  tree.applyInstanceOverrides(overrides);
}

}  // namespace Blunder
