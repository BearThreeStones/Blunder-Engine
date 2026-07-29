#pragma once

#include "EASTL/hash_map.h"
#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

using AnimationClipResolveFn = bool (*)(void* userdata, const eastl::string& guid,
                                        AnimationClipData& out_clip);

class AnimationPlayer {
 public:
  size_t getClipMapEntryCount() const { return m_name_to_guid.size(); }

  void setClipGuid(const eastl::string& name, const eastl::string& guid);
  bool getClipGuid(const eastl::string& name, eastl::string& out_guid) const;
  void clearClipGuid(const eastl::string& name);
  void clearAllClipGuids();

  void setClipResolver(AnimationClipResolveFn resolver, void* userdata);
  void injectClipData(const eastl::string& guid, AnimationClipData clip);

  bool play(const eastl::string& name);
  void stop();

  bool isPlaying() const { return m_playing; }
  void setLoop(bool loop) { m_loop = loop; }
  bool isLooping() const { return m_loop; }

  const eastl::string& getCurrentClipName() const { return m_current_clip_name; }
  float getPlaybackPosition() const { return m_position; }
  float getClipLength() const { return m_clip_length; }

  void advance(float delta_seconds);

 private:
  bool resolveClip(const eastl::string& guid, AnimationClipData& out_clip);
  void beginClip(const eastl::string& name, const AnimationClipData& clip);

  eastl::hash_map<eastl::string, eastl::string> m_name_to_guid;
  eastl::hash_map<eastl::string, AnimationClipData> m_injected_clips;
  AnimationClipResolveFn m_resolver{nullptr};
  void* m_resolver_userdata{nullptr};

  eastl::string m_current_clip_name;
  float m_position{0.0f};
  float m_clip_length{0.0f};
  bool m_playing{false};
  bool m_loop{false};
};

}  // namespace Blunder
