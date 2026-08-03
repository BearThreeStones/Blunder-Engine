#pragma once

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class AnimationPlayer;
class Skeleton;

struct BlendSpace1DPoint {
  eastl::string clip_name;
  float scalar{0.0f};
};

class AnimationTree {
 public:
  void bindAnimationPlayer(AnimationPlayer* player);
  bool hasAnimationPlayer() const { return m_animation_player != nullptr; }

  void bindSamplingSkeleton(Skeleton* skeleton);

  bool isActive() const { return m_active; }
  bool setActive(bool active);

  /// Minimal single-clip sample path (task 1.2); superseded by base BlendSpace1D when set.
  bool setSampleClipName(const eastl::string& name);
  const eastl::string& getSampleClipName() const { return m_sample_clip_name; }

  bool setAdd2ClipName(const eastl::string& name);
  const eastl::string& getAdd2ClipName() const { return m_add2_clip_name; }
  void setAdd2Weight(float weight);
  float getAdd2Weight() const { return m_add2_weight; }
  void setAdd2Time(float time) { m_add2_time = time; }
  float getAdd2Time() const { return m_add2_time; }

  void setSampleTime(float time) { m_sample_time = time; }
  float getSampleTime() const { return m_sample_time; }

  /// BlendSpace1D: discrete clip points along one scalar per node logical name.
  bool addBlendSpacePoint(const eastl::string& node_name,
                          const eastl::string& clip_name, float scalar);
  void clearBlendSpacePoints(const eastl::string& node_name);
  void setBlendSpaceScalar(const eastl::string& node_name, float scalar);
  float getBlendSpaceScalar(const eastl::string& node_name) const;

  bool setBaseBlendSpaceNode(const eastl::string& node_name);
  const eastl::string& getBaseBlendSpaceNode() const {
    return m_base_blend_space_node;
  }
  void clearBaseBlendSpaceNode() { m_base_blend_space_node.clear(); }

  void sampleOntoSkeleton(Skeleton& skeleton);
  void sampleBoundSkeleton();

  /// Resolve a clip logical name to its GUID via the co-located AnimationPlayer map.
  bool resolveClipGuid(const eastl::string& name, eastl::string& out_guid) const;

  /// Resolve clip data for a mapped logical name (injected clips or resolver).
  bool resolveClipForName(const eastl::string& name,
                          AnimationClipData& out_clip) const;

 private:
  void syncPlayerSamplingBlock();
  void sampleBaseOntoSkeleton(Skeleton& skeleton);
  bool sampleBlendSpace1DOntoSkeleton(Skeleton& skeleton,
                                    const eastl::string& node_name,
                                    float scalar);

  AnimationPlayer* m_animation_player{nullptr};
  Skeleton* m_sampling_skeleton{nullptr};
  bool m_active{false};
  eastl::string m_sample_clip_name;
  float m_sample_time{0.0f};
  eastl::string m_add2_clip_name;
  float m_add2_weight{0.0f};
  float m_add2_time{0.0f};
  eastl::hash_map<eastl::string, eastl::vector<BlendSpace1DPoint>> m_blend_spaces;
  eastl::hash_map<eastl::string, float> m_blend_space_scalars;
  eastl::string m_base_blend_space_node;
};

}  // namespace Blunder
