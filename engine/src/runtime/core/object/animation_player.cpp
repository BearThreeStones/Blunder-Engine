#include "runtime/core/object/animation_player.h"

#include "runtime/core/object/animation_sampler.h"
#include "runtime/core/object/skeleton.h"

#include <cmath>

namespace Blunder {

void AnimationPlayer::setClipGuid(const eastl::string& name,
                                  const eastl::string& guid) {
  if (name.empty()) {
    return;
  }
  m_name_to_guid[name] = guid;
}

bool AnimationPlayer::getClipGuid(const eastl::string& name,
                                  eastl::string& out_guid) const {
  const auto it = m_name_to_guid.find(name);
  if (it == m_name_to_guid.end()) {
    return false;
  }
  out_guid = it->second;
  return true;
}

void AnimationPlayer::clearClipGuid(const eastl::string& name) {
  m_name_to_guid.erase(name);
}

void AnimationPlayer::clearAllClipGuids() { m_name_to_guid.clear(); }

void AnimationPlayer::setClipResolver(AnimationClipResolveFn resolver,
                                      void* userdata) {
  m_resolver = resolver;
  m_resolver_userdata = userdata;
}

void AnimationPlayer::injectClipData(const eastl::string& guid,
                                     AnimationClipData clip) {
  if (guid.empty()) {
    return;
  }
  m_injected_clips[guid] = eastl::move(clip);
}

bool AnimationPlayer::resolveClip(const eastl::string& guid,
                                  AnimationClipData& out_clip) {
  const auto injected = m_injected_clips.find(guid);
  if (injected != m_injected_clips.end()) {
    out_clip = injected->second;
    return true;
  }
  if (m_resolver != nullptr) {
    return m_resolver(m_resolver_userdata, guid, out_clip);
  }
  return false;
}

void AnimationPlayer::beginClip(const eastl::string& name,
                                const AnimationClipData& clip) {
  m_current_clip_name = name;
  m_current_clip = clip;
  m_has_current_clip = true;
  m_clip_length = clip.duration;
  m_position = 0.0f;
  m_playing = true;
  sampleBoundSkeleton();
}

void AnimationPlayer::bindSamplingSkeleton(Skeleton* skeleton) {
  m_sampling_skeleton = skeleton;
}

void AnimationPlayer::addPoseAppliedListener(PoseAppliedFn fn, void* userdata) {
  if (fn == nullptr) {
    return;
  }
  m_pose_applied_listeners.push_back(PoseAppliedListener{fn, userdata});
}

void AnimationPlayer::clearPoseAppliedListeners() {
  m_pose_applied_listeners.clear();
}

void AnimationPlayer::notifyPoseApplied() {
  for (const PoseAppliedListener& listener : m_pose_applied_listeners) {
    if (listener.fn != nullptr) {
      listener.fn(*this, listener.userdata);
    }
  }
}

void AnimationPlayer::sampleOntoSkeleton(Skeleton& skeleton) {
  if (!m_has_current_clip) {
    return;
  }
  sampleClipOntoSkeleton(skeleton, m_current_clip, m_position);
  notifyPoseApplied();
}

void AnimationPlayer::sampleBoundSkeleton() {
  if (m_sampling_skeleton != nullptr && m_has_current_clip && m_playing) {
    sampleOntoSkeleton(*m_sampling_skeleton);
  }
}

bool AnimationPlayer::play(const eastl::string& name) {
  eastl::string guid;
  if (!getClipGuid(name, guid)) {
    return false;
  }

  AnimationClipData clip;
  if (!resolveClip(guid, clip)) {
    return false;
  }

  beginClip(name, clip);
  return true;
}

void AnimationPlayer::stop() {
  m_playing = false;
  m_position = 0.0f;
}

bool AnimationPlayer::setSlot(int slot_index, const eastl::string& name) {
  if (slot_index < 0 || slot_index > 1) {
    return false;
  }
  eastl::string guid;
  if (!getClipGuid(name, guid)) {
    return false;
  }
  m_slot_clip_names[slot_index] = name;
  return true;
}

const eastl::string& AnimationPlayer::getSlotClipName(int slot_index) const {
  static const eastl::string k_empty;
  if (slot_index < 0 || slot_index > 1) {
    return k_empty;
  }
  return m_slot_clip_names[slot_index];
}

void AnimationPlayer::setBlendWeight(float weight) {
  m_blend_weight = clamp01(weight);
}

float AnimationPlayer::clamp01(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

void AnimationPlayer::setTimeScale(float scale) { m_time_scale = scale; }

void AnimationPlayer::advance(float delta_seconds) {
  if (!m_playing || delta_seconds <= 0.0f || m_clip_length <= 0.0f) {
    return;
  }

  m_position += delta_seconds;
  if (m_position < m_clip_length) {
    sampleBoundSkeleton();
    return;
  }

  if (m_loop) {
    m_position = std::fmod(m_position, m_clip_length);
    if (m_position < 0.0f) {
      m_position += m_clip_length;
    }
    sampleBoundSkeleton();
    return;
  }

  m_position = m_clip_length;
  sampleBoundSkeleton();
  m_playing = false;
}

}  // namespace Blunder
