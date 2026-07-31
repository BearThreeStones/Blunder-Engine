#pragma once

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class Skeleton;
class AnimationPlayer;

using AnimationClipResolveFn = bool (*)(void* userdata, const eastl::string& guid,
                                        AnimationClipData& out_clip);
using PoseAppliedFn = void (*)(AnimationPlayer& player, void* userdata);

class AnimationPlayer {
 public:
  struct ClipBinding final {
    eastl::string name;
    eastl::string guid;
  };

  size_t getClipMapEntryCount() const { return m_name_to_guid.size(); }

  void setClipGuid(const eastl::string& name, const eastl::string& guid);
  bool getClipGuid(const eastl::string& name, eastl::string& out_guid) const;
  void clearClipGuid(const eastl::string& name);
  void clearAllClipGuids();
  eastl::vector<ClipBinding> getClipBindings() const;
  void setClipBindings(const eastl::vector<ClipBinding>& bindings);

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

  void addPoseAppliedListener(PoseAppliedFn fn, void* userdata);
  void clearPoseAppliedListeners();

  /// Co-located Skeleton only (set by Object). When bound, play/advance sample poses.
  void bindSamplingSkeleton(Skeleton* skeleton);
  void sampleOntoSkeleton(Skeleton& skeleton);

 private:
  void notifyPoseApplied();

  struct PoseAppliedListener {
    PoseAppliedFn fn{nullptr};
    void* userdata{nullptr};
  };

  bool resolveClip(const eastl::string& guid, AnimationClipData& out_clip);
  void beginClip(const eastl::string& name, const AnimationClipData& clip);
  void sampleBoundSkeleton();

  eastl::hash_map<eastl::string, eastl::string> m_name_to_guid;
  eastl::hash_map<eastl::string, AnimationClipData> m_injected_clips;
  AnimationClipResolveFn m_resolver{nullptr};
  void* m_resolver_userdata{nullptr};

  eastl::string m_current_clip_name;
  AnimationClipData m_current_clip;
  bool m_has_current_clip{false};
  Skeleton* m_sampling_skeleton{nullptr};
  float m_position{0.0f};
  float m_clip_length{0.0f};
  bool m_playing{false};
  bool m_loop{false};
  eastl::vector<PoseAppliedListener> m_pose_applied_listeners;
};

}  // namespace Blunder
