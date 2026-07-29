#include "runtime/core/object/animation_player.h"

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
  m_clip_length = clip.duration;
  m_position = 0.0f;
  m_playing = true;
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
    return;
  }

  if (m_loop) {
    m_position = std::fmod(m_position, m_clip_length);
    if (m_position < 0.0f) {
      m_position += m_clip_length;
    }
    return;
  }

  m_position = m_clip_length;
  m_playing = false;
}

}  // namespace Blunder
