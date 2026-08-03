#pragma once

#include "EASTL/string.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AnimationPlayer;
class Skeleton;

class AnimationTree {
 public:
  void bindAnimationPlayer(AnimationPlayer* player);
  bool hasAnimationPlayer() const { return m_animation_player != nullptr; }

  void bindSamplingSkeleton(Skeleton* skeleton);

  bool isActive() const { return m_active; }
  bool setActive(bool active);

  /// Minimal single-clip sample path (task 1.2); full graph nodes land in 1.3–1.6.
  bool setSampleClipName(const eastl::string& name);
  const eastl::string& getSampleClipName() const { return m_sample_clip_name; }

  void setSampleTime(float time) { m_sample_time = time; }
  float getSampleTime() const { return m_sample_time; }

  void sampleOntoSkeleton(Skeleton& skeleton);
  void sampleBoundSkeleton();

  /// Resolve a clip logical name to its GUID via the co-located AnimationPlayer map.
  bool resolveClipGuid(const eastl::string& name, eastl::string& out_guid) const;

  /// Resolve clip data for a mapped logical name (injected clips or resolver).
  bool resolveClipForName(const eastl::string& name,
                          AnimationClipData& out_clip) const;

 private:
  void syncPlayerSamplingBlock();

  AnimationPlayer* m_animation_player{nullptr};
  Skeleton* m_sampling_skeleton{nullptr};
  bool m_active{false};
  eastl::string m_sample_clip_name;
  float m_sample_time{0.0f};
};

}  // namespace Blunder
