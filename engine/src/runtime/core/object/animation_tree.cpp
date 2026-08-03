#include "runtime/core/object/animation_tree.h"

#include "runtime/core/object/animation_player.h"

namespace Blunder {

void AnimationTree::bindAnimationPlayer(AnimationPlayer* player) {
  m_animation_player = player;
}

bool AnimationTree::resolveClipGuid(const eastl::string& name,
                                    eastl::string& out_guid) const {
  if (m_animation_player == nullptr) {
    return false;
  }
  return m_animation_player->getClipGuid(name, out_guid);
}

bool AnimationTree::resolveClipForName(const eastl::string& name,
                                     AnimationClipData& out_clip) const {
  if (m_animation_player == nullptr) {
    return false;
  }
  return m_animation_player->resolveClipForName(name, out_clip);
}

}  // namespace Blunder
