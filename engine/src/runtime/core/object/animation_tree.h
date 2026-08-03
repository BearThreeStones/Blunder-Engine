#pragma once

#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AnimationPlayer;

class AnimationTree {
 public:
  void bindAnimationPlayer(AnimationPlayer* player);
  bool hasAnimationPlayer() const { return m_animation_player != nullptr; }

  /// Resolve a clip logical name to its GUID via the co-located AnimationPlayer map.
  bool resolveClipGuid(const eastl::string& name, eastl::string& out_guid) const;

  /// Resolve clip data for a mapped logical name (injected clips or resolver).
  bool resolveClipForName(const eastl::string& name,
                          AnimationClipData& out_clip) const;

 private:
  AnimationPlayer* m_animation_player{nullptr};
};

}  // namespace Blunder
