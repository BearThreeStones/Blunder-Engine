#include "runtime/core/object/animation_tree.h"

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sampler.h"
#include "runtime/core/object/skeleton.h"

namespace Blunder {

void AnimationTree::bindAnimationPlayer(AnimationPlayer* player) {
  m_animation_player = player;
  syncPlayerSamplingBlock();
}

void AnimationTree::bindSamplingSkeleton(Skeleton* skeleton) {
  m_sampling_skeleton = skeleton;
}

bool AnimationTree::setActive(bool active) {
  m_active = active;
  syncPlayerSamplingBlock();
  if (m_active) {
    sampleBoundSkeleton();
  } else if (m_animation_player != nullptr) {
    m_animation_player->resampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::setSampleClipName(const eastl::string& name) {
  if (name.empty()) {
    m_sample_clip_name.clear();
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(name, guid)) {
    return false;
  }
  m_sample_clip_name = name;
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

void AnimationTree::sampleOntoSkeleton(Skeleton& skeleton) {
  if (!m_active || m_sample_clip_name.empty()) {
    return;
  }
  AnimationClipData clip;
  if (!resolveClipForName(m_sample_clip_name, clip)) {
    return;
  }
  sampleClipOntoSkeleton(skeleton, clip, m_sample_time);
}

void AnimationTree::sampleBoundSkeleton() {
  if (m_sampling_skeleton != nullptr && m_active) {
    sampleOntoSkeleton(*m_sampling_skeleton);
  }
}

void AnimationTree::syncPlayerSamplingBlock() {
  if (m_animation_player != nullptr) {
    m_animation_player->setTreeBlocksSampling(m_active);
  }
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
