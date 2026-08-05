#pragma once

#include "EASTL/string.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_id.h"
#include "runtime/function/scene/entity_id.h"
#include "runtime/resource/asset/asset_descriptor.h"

namespace Blunder {

class Object;
class SceneInstance;

enum class AnimationPreviewState : uint8_t {
  Stopped = 0,
  Playing,
  Paused,
};

/// Edit Mode AnimationPlayer preview — no DotNetHost / Behaviour Tick.
class AnimationPreviewController final {
 public:
  bool hasTarget() const { return m_target_object != nullptr; }
  AnimationPreviewState state() const { return m_state; }

  bool playEnabled() const;
  bool pauseEnabled() const;
  bool stopEnabled() const;
  bool isLooping() const;
  bool isPaused() const { return m_state == AnimationPreviewState::Paused; }

  const eastl::string& defaultClipName() const { return m_default_clip_name; }
  float playbackPosition() const;
  float clipLength() const;

  float timeScale() const;
  float blendWeight() const;
  float fadeSeconds() const { return m_fade_seconds; }
  const eastl::string& slotClipName(int slot_index) const;

  bool hasTree() const;
  bool isTreeActive() const;
  float blendSpaceScalar(const eastl::string& node_name) const;
  bool blendSpace2DParam(const eastl::string& node_name, float& out_x,
                         float& out_y) const;
  float add2Weight() const;
  eastl::string assetGuid() const;

  void bindObject(Object* object, const eastl::string& default_clip_name = {});
  void bindSelection(SceneInstance* scene, EntityId entity_id);
  void clearTarget();

  bool play(const eastl::string& clip_name = {});
  bool pause();
  bool resume();
  void stop();
  void toggleLoop();
  void setLoop(bool loop);

  void setTimeScale(float scale);
  void setBlendWeight(float weight);
  void setFadeSeconds(float fade_seconds);
  bool setSlot(int slot_index, const eastl::string& name);

  bool setTreeActive(bool active);
  bool travel(const eastl::string& state_name);
  bool start(const eastl::string& state_name);
  void setBlendSpaceScalar(const eastl::string& node_name, float scalar);
  void setBlendSpace2DParam(const eastl::string& node_name, float x, float y);
  bool requestOneShot(const eastl::string& clip_name);
  void setAdd2Weight(float weight);
  bool setAdd2ClipName(const eastl::string& name);

  void setAssetGuid(const eastl::string& guid);
  bool applyTreeTopology(const AnimationTreeTopologyData& topology);
  void applyTreeOverrides(const AnimationTreeInstanceOverrides& overrides);

  size_t skeletonModifierCount() const;
  bool setSkeletonModifierEnabled(size_t index, bool enabled);
  bool isSkeletonModifierEnabled(size_t index) const;
  bool moveSkeletonModifier(size_t from_index, size_t to_index);
  bool setSkeletonLookAtTarget(size_t modifier_index, const Vec3& target);
  bool setSkeletonLookAtBoneName(size_t modifier_index,
                                 const eastl::string& bone_name);
  bool setSkeletonPaperMouthOpenAmount(size_t modifier_index, float open_amount);
  bool setSkeletonAttachBoneName(size_t modifier_index,
                                 const eastl::string& bone_name);
  bool setSkeletonAttachChildObjectId(size_t modifier_index,
                                      ObjectId child_object_id);

  /// Advance preview playback via tickObjectAnimationPreviewFrame when playing.
  void tick(float delta_time);

 private:
  void resampleBoundSkeleton();

  Object* m_target_object{nullptr};
  eastl::string m_default_clip_name;
  float m_fade_seconds{0.0f};
  AnimationPreviewState m_state{AnimationPreviewState::Stopped};
};

}  // namespace Blunder
