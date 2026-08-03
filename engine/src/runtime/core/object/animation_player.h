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
using FinishedFn = void (*)(AnimationPlayer& player, void* userdata);

class AnimationPlayer {
 public:
  struct ClipBinding final {
    eastl::string name;
    eastl::string guid;
  };

  size_t getClipMapEntryCount() const { return m_name_to_guid.size(); }

  using ClipBindingVisitorFn = void (*)(const eastl::string& name,
                                        const eastl::string& guid, void* userdata);
  void visitClipBindings(ClipBindingVisitorFn visitor, void* userdata) const;

  void setClipGuid(const eastl::string& name, const eastl::string& guid);
  bool getClipGuid(const eastl::string& name, eastl::string& out_guid) const;
  void clearClipGuid(const eastl::string& name);
  void clearAllClipGuids();
  eastl::vector<ClipBinding> getClipBindings() const;
  void setClipBindings(const eastl::vector<ClipBinding>& bindings);

  void setClipResolver(AnimationClipResolveFn resolver, void* userdata);
  void injectClipData(const eastl::string& guid, AnimationClipData clip);

  /// Resolve clip data for a mapped logical name (injected clips or resolver).
  bool resolveClipForName(const eastl::string& name, AnimationClipData& out_clip);

  bool play(const eastl::string& name);
  /// Play with optional crossfade. fade_seconds <= 0 is a hard cut (Phase 1).
  bool play(const eastl::string& name, float fade_seconds);
  /// Hard snap: clears crossfade and dual-slot blend, then plays \a name from the start.
  bool snapPlay(const eastl::string& name);
  /// Hard snap using already-resolved clip data (used by Sync Group Fire batch apply).
  void snapPlayWithClip(const eastl::string& name, const AnimationClipData& clip);
  void stop();

  bool isCrossfading() const { return m_crossfade_active; }

  /// Assign a mapped clip name to slot 0 or 1 (Phase 2 two-slot model).
  bool setSlot(int slot_index, const eastl::string& name);
  const eastl::string& getSlotClipName(int slot_index) const;

  void setBlendWeight(float weight);
  float getBlendWeight() const { return m_blend_weight; }

  void setTimeScale(float scale);
  float getTimeScale() const { return m_time_scale; }

  bool isPlaying() const { return m_playing; }
  void setLoop(bool loop) { m_loop = loop; }
  bool isLooping() const { return m_loop; }

  /// Seek the active clip to \a seconds (clamped). No-op if not playing.
  void seekPlayback(float seconds);

  const eastl::string& getCurrentClipName() const { return m_current_clip_name; }
  float getPlaybackPosition() const;
  float getClipLength() const { return m_clip_length; }

  void advance(float delta_seconds);

  void addPoseAppliedListener(PoseAppliedFn fn, void* userdata);
  void clearPoseAppliedListeners();

  /// Raised when a non-looping clip reaches its natural end (not on stop()).
  void addFinishedListener(FinishedFn fn, void* userdata);
  void clearFinishedListeners();

  /// Co-located Skeleton only (set by Object). When bound, play/advance sample poses.
  void bindSamplingSkeleton(Skeleton* skeleton);
  void sampleOntoSkeleton(Skeleton& skeleton);

 private:
  void notifyPoseApplied();
  void notifyFinished();

  struct PoseAppliedListener {
    PoseAppliedFn fn{nullptr};
    void* userdata{nullptr};
  };

  struct FinishedListener {
    FinishedFn fn{nullptr};
    void* userdata{nullptr};
  };

  bool resolveClip(const eastl::string& guid, AnimationClipData& out_clip);
  bool resolveSlotClip(int slot_index, AnimationClipData& out_clip) const;
  bool hasActiveSlot() const;
  void advanceSlot(int slot_index, float delta_seconds);
  void beginClip(const eastl::string& name, const AnimationClipData& clip);
  void sampleBoundSkeleton();
  void clearCrossfade();
  void advanceCrossfade(float delta_seconds);
  bool beginCrossfade(const eastl::string& name, float fade_seconds);
  int getDominantSlotIndex() const;
  static float clamp01(float value);
  static float lerp(float a, float b, float t);

  static constexpr int k_slot_count = 2;

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
  eastl::string m_slot_clip_names[k_slot_count];
  float m_slot_positions[k_slot_count]{0.0f, 0.0f};
  float m_blend_weight{0.0f};
  float m_time_scale{1.0f};
  bool m_crossfade_active{false};
  float m_crossfade_elapsed{0.0f};
  float m_crossfade_duration{0.0f};
  float m_crossfade_start_weight{0.0f};
  float m_crossfade_target_weight{0.0f};
  eastl::vector<PoseAppliedListener> m_pose_applied_listeners;
  eastl::vector<FinishedListener> m_finished_listeners;
};

}  // namespace Blunder
