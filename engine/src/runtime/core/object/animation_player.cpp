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

void AnimationPlayer::visitClipBindings(ClipBindingVisitorFn visitor,
                                        void* userdata) const {
  if (visitor == nullptr) {
    return;
  }
  for (const auto& entry : m_name_to_guid) {
    visitor(entry.first, entry.second, userdata);
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

bool AnimationPlayer::resolveClipForName(const eastl::string& name,
                                         AnimationClipData& out_clip) {
  eastl::string guid;
  if (!getClipGuid(name, guid)) {
    return false;
  }
  return resolveClip(guid, out_clip);
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

void AnimationPlayer::bindAnimationTree(AnimationTree* tree) {
  m_bound_tree = tree;
}

void AnimationPlayer::setTreeBlocksSampling(bool blocks) {
  m_tree_blocks_sampling = blocks;
  if (!blocks) {
    m_tree_playback_position = 0.0f;
    m_tree_clip_length = 0.0f;
  }
}

void AnimationPlayer::syncTreePlaybackClock(float position, float clip_length) {
  m_tree_playback_position = position;
  m_tree_clip_length = clip_length;
}

void AnimationPlayer::notifyPoseAppliedFromTree() { notifyPoseApplied(); }

void AnimationPlayer::resampleBoundSkeleton() {
  sampleBoundSkeleton();
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

void AnimationPlayer::addFinishedListener(FinishedFn fn, void* userdata) {
  if (fn == nullptr) {
    return;
  }
  m_finished_listeners.push_back(FinishedListener{fn, userdata});
}

void AnimationPlayer::clearFinishedListeners() {
  m_finished_listeners.clear();
}

void AnimationPlayer::notifyPoseApplied() {
  for (const PoseAppliedListener& listener : m_pose_applied_listeners) {
    if (listener.fn != nullptr) {
      listener.fn(*this, listener.userdata);
    }
  }
}

void AnimationPlayer::notifyFinished() {
  for (const FinishedListener& listener : m_finished_listeners) {
    if (listener.fn != nullptr) {
      listener.fn(*this, listener.userdata);
    }
  }
}

void AnimationPlayer::sampleOntoSkeleton(Skeleton& skeleton) {
  if (m_tree_blocks_sampling) {
    return;
  }
  AnimationClipData clip0;
  AnimationClipData clip1;
  const bool has_slot0 = resolveSlotClip(0, clip0);
  const bool has_slot1 = resolveSlotClip(1, clip1);

  if (has_slot0 && has_slot1) {
    blendClipsOntoSkeleton(skeleton, clip0, m_slot_positions[0], clip1,
                           m_slot_positions[1], m_blend_weight);
    notifyPoseApplied();
    return;
  }
  if (has_slot0) {
    sampleClipOntoSkeleton(skeleton, clip0, m_slot_positions[0]);
    notifyPoseApplied();
    return;
  }
  if (has_slot1) {
    sampleClipOntoSkeleton(skeleton, clip1, m_slot_positions[1]);
    notifyPoseApplied();
    return;
  }

  if (!m_has_current_clip) {
    return;
  }
  sampleClipOntoSkeleton(skeleton, m_current_clip, m_position);
  notifyPoseApplied();
}

void AnimationPlayer::sampleBoundSkeleton() {
  if (m_sampling_skeleton == nullptr || !m_playing || m_tree_blocks_sampling) {
    return;
  }
  if (hasActiveSlot() || m_has_current_clip) {
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

  clearCrossfade();
  beginClip(name, clip);
  return true;
}

bool AnimationPlayer::snapPlay(const eastl::string& name) {
  AnimationClipData clip;
  if (!resolveClipForName(name, clip)) {
    return false;
  }

  snapPlayWithClip(name, clip);
  return true;
}

void AnimationPlayer::snapPlayWithClip(const eastl::string& name,
                                       const AnimationClipData& clip) {
  clearCrossfade();
  m_slot_clip_names[0].clear();
  m_slot_clip_names[1].clear();
  m_slot_positions[0] = 0.0f;
  m_slot_positions[1] = 0.0f;
  m_blend_weight = 0.0f;
  beginClip(name, clip);
}

void AnimationPlayer::stop() {
  m_playing = false;
  m_position = 0.0f;
  m_slot_positions[0] = 0.0f;
  m_slot_positions[1] = 0.0f;
  clearCrossfade();
}

bool AnimationPlayer::resolveSlotClip(int slot_index,
                                      AnimationClipData& out_clip) const {
  if (slot_index < 0 || slot_index >= k_slot_count) {
    return false;
  }
  const eastl::string& name = m_slot_clip_names[slot_index];
  if (name.empty()) {
    return false;
  }
  eastl::string guid;
  if (!getClipGuid(name, guid)) {
    return false;
  }
  return const_cast<AnimationPlayer*>(this)->resolveClip(guid, out_clip);
}

bool AnimationPlayer::hasActiveSlot() const {
  for (int slot = 0; slot < k_slot_count; ++slot) {
    if (!m_slot_clip_names[slot].empty()) {
      return true;
    }
  }
  return false;
}

void AnimationPlayer::advanceSlot(int slot_index, float delta_seconds) {
  AnimationClipData clip;
  if (!resolveSlotClip(slot_index, clip) || clip.duration <= 0.0f) {
    return;
  }

  float& position = m_slot_positions[slot_index];
  position += delta_seconds;
  if (position < clip.duration) {
    return;
  }

  if (m_loop) {
    position = std::fmod(position, clip.duration);
    if (position < 0.0f) {
      position += clip.duration;
    }
    return;
  }

  position = clip.duration;
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
  m_slot_positions[slot_index] = 0.0f;
  return true;
}

const eastl::string& AnimationPlayer::getSlotClipName(int slot_index) const {
  static const eastl::string k_empty;
  if (slot_index < 0 || slot_index > 1) {
    return k_empty;
  }
  return m_slot_clip_names[slot_index];
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

float AnimationPlayer::lerp(float a, float b, float t) {
  return a + (b - a) * clamp01(t);
}

void AnimationPlayer::clearCrossfade() {
  m_crossfade_active = false;
  m_crossfade_elapsed = 0.0f;
  m_crossfade_duration = 0.0f;
  m_crossfade_start_weight = 0.0f;
  m_crossfade_target_weight = 0.0f;
}

void AnimationPlayer::advanceCrossfade(float delta_seconds) {
  if (!m_crossfade_active || m_crossfade_duration <= 0.0f) {
    return;
  }

  m_crossfade_elapsed += delta_seconds;
  const float t = m_crossfade_elapsed / m_crossfade_duration;
  m_blend_weight =
      lerp(m_crossfade_start_weight, m_crossfade_target_weight, t);
  if (m_crossfade_elapsed >= m_crossfade_duration) {
    m_blend_weight = m_crossfade_target_weight;
    m_crossfade_active = false;
  }
}

bool AnimationPlayer::beginCrossfade(const eastl::string& name,
                                     float fade_seconds) {
  eastl::string guid;
  if (!getClipGuid(name, guid)) {
    return false;
  }

  AnimationClipData clip;
  if (!resolveClip(guid, clip)) {
    return false;
  }

  if (!hasActiveSlot() && m_has_current_clip) {
    m_slot_clip_names[0] = m_current_clip_name;
    m_slot_positions[0] = m_position;
  }

  const int target_slot = (m_blend_weight < 0.5f) ? 1 : 0;
  const int source_slot = 1 - target_slot;
  const float target_weight = (target_slot == 1) ? 1.0f : 0.0f;

  if (!setSlot(target_slot, name)) {
    return false;
  }

  if (m_slot_clip_names[source_slot].empty() && m_has_current_clip) {
    m_slot_clip_names[source_slot] = m_current_clip_name;
    m_slot_positions[source_slot] = m_position;
  }

  m_current_clip_name = name;
  m_current_clip = clip;
  m_has_current_clip = true;
  m_clip_length = clip.duration;
  m_playing = true;

  m_crossfade_active = true;
  m_crossfade_elapsed = 0.0f;
  m_crossfade_duration = fade_seconds;
  m_crossfade_start_weight = m_blend_weight;
  m_crossfade_target_weight = target_weight;

  sampleBoundSkeleton();
  return true;
}

bool AnimationPlayer::play(const eastl::string& name, float fade_seconds) {
  if (fade_seconds <= 0.0f) {
    return play(name);
  }
  return beginCrossfade(name, fade_seconds);
}

void AnimationPlayer::setBlendWeight(float weight) {
  m_blend_weight = clamp01(weight);
}

void AnimationPlayer::setTimeScale(float scale) { m_time_scale = scale; }

int AnimationPlayer::getDominantSlotIndex() const {
  if (m_crossfade_active) {
    // While crossfading, step sync follows the fade target slot.
    return (m_crossfade_target_weight >= 0.5f) ? 1 : 0;
  }
  // Higher blendWeight favors slot1; tie at 0.5 prefers slot0.
  return (m_blend_weight > 0.5f) ? 1 : 0;
}

float AnimationPlayer::getPlaybackPosition() const {
  if (m_tree_blocks_sampling) {
    return m_tree_playback_position;
  }
  if (hasActiveSlot()) {
    return m_slot_positions[getDominantSlotIndex()];
  }
  return m_position;
}

void AnimationPlayer::seekPlayback(float seconds) {
  if (!m_playing) {
    return;
  }

  if (seconds < 0.0f) {
    seconds = 0.0f;
  }
  if (m_clip_length > 0.0f && seconds > m_clip_length) {
    seconds = m_clip_length;
  }

  if (hasActiveSlot()) {
    m_slot_positions[getDominantSlotIndex()] = seconds;
  } else {
    m_position = seconds;
  }

  sampleBoundSkeleton();
}

void AnimationPlayer::advance(float delta_seconds) {
  if (!m_playing || delta_seconds <= 0.0f) {
    return;
  }

  const float scaled_delta = delta_seconds * m_time_scale;
  if (scaled_delta <= 0.0f) {
    return;
  }

  advanceCrossfade(scaled_delta);

  if (hasActiveSlot()) {
    for (int slot = 0; slot < k_slot_count; ++slot) {
      if (!m_slot_clip_names[slot].empty()) {
        advanceSlot(slot, scaled_delta);
      }
    }
    sampleBoundSkeleton();
    return;
  }

  if (m_clip_length <= 0.0f) {
    return;
  }

  m_position += scaled_delta;
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
  notifyFinished();
}

}  // namespace Blunder
