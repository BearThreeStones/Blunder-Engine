#include "runtime/core/object/animation_player.h"

#include "runtime/core/object/animation_sampler.h"
#include "runtime/core/object/skeleton.h"

#include <algorithm>
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

eastl::vector<AnimationPlayer::ClipBinding> AnimationPlayer::getClipBindings()
    const {
  eastl::vector<ClipBinding> bindings;
  bindings.reserve(m_name_to_guid.size());
  for (const auto& entry : m_name_to_guid) {
    ClipBinding binding;
    binding.name = entry.first;
    binding.guid = entry.second;
    bindings.push_back(eastl::move(binding));
  }
  std::sort(bindings.begin(), bindings.end(),
            [](const ClipBinding& a, const ClipBinding& b) {
              return a.name < b.name;
            });
  return bindings;
}

void AnimationPlayer::setClipBindings(
    const eastl::vector<ClipBinding>& bindings) {
  m_name_to_guid.clear();
  for (const ClipBinding& binding : bindings) {
    if (!binding.name.empty()) {
      m_name_to_guid[binding.name] = binding.guid;
    }
  }
}

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
