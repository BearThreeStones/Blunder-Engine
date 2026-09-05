#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"
#include "runtime/core/object/object_id.h"
#include "runtime/function/editor/animation_clip_anatomy.h"
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

/// Edit Mode animation preview — no DotNetHost / Behaviour Tick.
/// Window path binds an AnimationTree. Player helpers remain for Phase 2 tests.
class AnimationPreviewController final {
 public:
  bool hasTarget() const { return m_target_object != nullptr; }
  AnimationPreviewState state() const { return m_state; }

  bool playEnabled() const;
  bool pauseEnabled() const;
  bool stopEnabled() const;
  bool isLooping() const;
  bool isPaused() const { return m_state == AnimationPreviewState::Paused; }
  bool isPlaying() const { return m_state == AnimationPreviewState::Playing; }
  bool windowBound() const;

  const eastl::string& defaultClipName() const { return m_default_clip_name; }
  float playbackPosition() const;
  float clipLength() const;
  eastl::string rulerClipName() const;
  eastl::string clockReadout() const;

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
  void bindSelection(SceneInstance* scene, EntityId entity_id,
                     size_t selected_count);
  void clearTarget();

  /// Clear Clip Play, seek 0, clear Fire, End CINE. Unbind and Play Session
  /// start use this; window Stop does not.
  void haltBoundSession();

  bool play(const eastl::string& clip_name = {});
  bool pause();
  bool resume();
  void stop();
  void toggleLoop();
  void setLoop(bool loop);

  void seekPlayback(float seconds);

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

  eastl::vector<eastl::string> fireClipNames() const;
  const eastl::string& previewClip() const { return m_preview_clip; }
  const eastl::string& fireTarget() const { return previewClip(); }
  void setPreviewClip(const eastl::string& clip_name);
  void setFireTarget(const eastl::string& clip_name) {
    setPreviewClip(clip_name);
  }
  bool fire();

  /// Clip anatomy of the current ruler clip; rebuilt when that clip changes.
  void refreshClipAnatomy();
  const AnimationClipAnatomy& clipAnatomy() const { return m_anatomy; }
  eastl::vector<AnimationAnatomyRow> anatomyRows() const;
  /// Bumped whenever the rows change; lets the window skip identical rebuilds.
  uint32_t anatomyRevision() const { return m_anatomy_revision; }
  const eastl::string& anatomyFilter() const {
    return m_anatomy_session.filter();
  }
  void setAnatomyFilter(const eastl::string& query);
  void toggleAnatomyGroup(const eastl::string& bone_name);
  bool isAnatomyGroupCollapsed(const eastl::string& bone_name) const;

  bool isInCine() const { return m_in_cine; }
  bool isInputSuppressed() const { return m_input_suppressed; }
  void enterCine();
  void endCine();

  void setAssetGuid(const eastl::string& guid);
  bool applyTreeTopology(const AnimationTreeTopologyData& topology);
  void applyTreeOverrides(const AnimationTreeInstanceOverrides& overrides);

  size_t skeletonModifierCount() const;
  bool addSkeletonModifier(const eastl::string& type_name);
  bool removeSkeletonModifier(size_t index);
  bool setSkeletonModifierEnabled(size_t index, bool enabled);
  bool isSkeletonModifierEnabled(size_t index) const;
  bool moveSkeletonModifier(size_t from_index, size_t to_index);
  bool setSkeletonLookAtTarget(size_t modifier_index, const Vec3& target);
  bool setSkeletonLookAtBoneName(size_t modifier_index,
                                 const eastl::string& bone_name);
  bool setSkeletonPaperMouthOpenAmount(size_t modifier_index, float open_amount);
  bool setSkeletonPaperMouthBoneName(size_t modifier_index,
                                     const eastl::string& bone_name);
  bool setSkeletonAttachBoneName(size_t modifier_index,
                                 const eastl::string& bone_name);
  bool setSkeletonAttachChildObjectId(size_t modifier_index,
                                      ObjectId child_object_id);

  /// Advance preview playback via tickObjectAnimationPreviewFrame when playing.
  void tick(float delta_time);

  void resampleBoundSkeleton();

 private:
  void resetSessionChrome();
  bool atRulerEnd() const;
  void defaultPreviewClipFromBindings();
  void applyPreviewClipPlay();

  Object* m_target_object{nullptr};
  eastl::string m_default_clip_name;
  float m_fade_seconds{0.0f};
  AnimationPreviewState m_state{AnimationPreviewState::Stopped};
  bool m_session_loop{false};
  eastl::string m_preview_clip;
  AnimationAnatomySession m_anatomy_session;
  AnimationClipAnatomy m_anatomy;
  eastl::string m_anatomy_clip_guid;
  uint32_t m_anatomy_revision{0};
  bool m_in_cine{false};
  bool m_input_suppressed{false};
};

}  // namespace Blunder
