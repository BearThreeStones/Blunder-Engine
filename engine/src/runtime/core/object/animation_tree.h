#pragma once

#include "EASTL/hash_map.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/resource/asset/asset_descriptor.h"

#include "runtime/core/object/skeleton_modifier.h"

namespace Blunder {

class AnimationPlayer;
class Skeleton;

struct BlendSpace1DPoint {
  eastl::string clip_name;
  float scalar{0.0f};
};

struct BlendSpace2DPoint {
  eastl::string clip_name;
  float x{0.0f};
  float y{0.0f};
};

struct BlendSpace2DParam {
  float x{0.0f};
  float y{0.0f};
};

enum class AnimationStatePlaybackKind {
  Clip,
  BlendSpace1D,
  BlendSpace2D,
};

struct AnimationStateDefinition {
  AnimationStatePlaybackKind kind{AnimationStatePlaybackKind::Clip};
  eastl::string clip_name;
  eastl::string blend_space_node;
};

enum class TransitionConditionSource {
  TreeParam,
  BlendSpace1DScalar,
  BlendSpace2DX,
  BlendSpace2DY,
  Add2Weight,
};

enum class TransitionCompareOp {
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
};

/// Authored StateMachine edge (Phase 7). Bool predicates use truth check /
/// bool_operand; float predicates use op + float_operand.
struct StateMachineTransition {
  eastl::string from_state;
  eastl::string to_state;
  TransitionConditionSource source{TransitionConditionSource::TreeParam};
  eastl::string param_name;
  bool is_bool_predicate{false};
  TransitionCompareOp op{TransitionCompareOp::Eq};
  float float_operand{0.0f};
  bool bool_operand{true};
  int priority{0};
};

class AnimationTree {
 public:
  void bindAnimationPlayer(AnimationPlayer* player);
  bool hasAnimationPlayer() const { return m_animation_player != nullptr; }

  void bindSamplingSkeleton(Skeleton* skeleton);
  void bindSkeletonModifierChain(SkeletonModifierChainFn fn, void* userdata);

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

  /// BlendSpace2D: clip points on a 2D parameter plane; triangulation + barycentric.
  bool addBlendSpace2DPoint(const eastl::string& node_name,
                            const eastl::string& clip_name, float x, float y);
  void clearBlendSpace2DPoints(const eastl::string& node_name);
  void setBlendSpace2DParam(const eastl::string& node_name, float x, float y);
  BlendSpace2DParam getBlendSpace2DParam(const eastl::string& node_name) const;

  bool setBaseBlendSpace2DNode(const eastl::string& node_name);
  const eastl::string& getBaseBlendSpace2DNode() const {
    return m_base_blend_space_2d_node;
  }
  void clearBaseBlendSpace2DNode() { m_base_blend_space_2d_node.clear(); }

  /// StateMachine: named states with single-clip or BlendSpace1D/2D playback.
  bool setStateClip(const eastl::string& state_name,
                    const eastl::string& clip_name);
  bool setStateBlendSpace(const eastl::string& state_name,
                          const eastl::string& blend_space_node);
  bool setStateBlendSpace2D(const eastl::string& state_name,
                            const eastl::string& blend_space_node);
  bool travel(const eastl::string& state_name);
  bool start(const eastl::string& state_name);
  const eastl::string& getCurrentStateName() const {
    return m_current_state_name;
  }

  /// Independent tree parameters (Phase 7) for transition conditions.
  void setTreeParamBool(const eastl::string& name, bool value);
  bool getTreeParamBool(const eastl::string& name) const;
  void setTreeParamFloat(const eastl::string& name, float value);
  float getTreeParamFloat(const eastl::string& name) const;

  bool addTransition(const StateMachineTransition& transition);
  void clearTransitions();
  using TransitionVisitor = void (*)(const StateMachineTransition& transition,
                                     void* userdata);
  void visitTransitions(TransitionVisitor visitor, void* userdata) const;

  void setCanvasNodePosition(const eastl::string& node_id, float x, float y);
  bool getCanvasNodePosition(const eastl::string& node_id, float& out_x,
                             float& out_y) const;
  using CanvasLayoutVisitor = void (*)(const eastl::string& node_id, float x,
                                       float y, void* userdata);
  void visitCanvasLayout(CanvasLayoutVisitor visitor, void* userdata) const;

  /// OneShot: insert a clip over the base graph, then return when finished.
  bool requestOneShot(const eastl::string& clip_name);
  bool isOneShotActive() const { return m_oneshot_active; }
  const eastl::string& getOneShotClipName() const { return m_oneshot_clip_name; }
  void clearOneShot();

  /// Clip Play: replace the tree base with a Clip Binding clip (hard cut).
  bool clipPlay(const eastl::string& clip_name);
  void clearClipPlay();
  bool isClipPlayOverride() const { return m_clip_play_active; }
  const eastl::string& getClipPlayClipName() const {
    return m_clip_play_clip_name;
  }
  float getClipPlayTime() const { return m_clip_play_time; }

  /// Edit Animation Window ruler: insert clip while OneShot occupies, else base dominant.
  float rulerPosition() const;
  float rulerLength() const;
  eastl::string rulerClipName() const;
  void seekRuler(float seconds);
  /// Authored OneShot clip slot (scene embed); does not start playback.
  bool setOneShotSlotClip(const eastl::string& clip_name);
  const eastl::string& getOneShotSlotClip() const { return m_oneshot_slot_clip; }

  /// AnimationTree Asset GUID reference (empty = embed-only Phase 4 path).
  void setAssetGuid(const eastl::string& guid) { m_asset_guid = guid; }
  const eastl::string& getAssetGuid() const { return m_asset_guid; }

  void clearAuthoredTopology();
  bool applyTopologyData(const AnimationTreeTopologyData& topology);
  void exportTopologyData(AnimationTreeTopologyData& out_topology) const;
  void applyInstanceOverrides(const AnimationTreeInstanceOverrides& overrides);

  using BlendSpaceVisitor = void (*)(const eastl::string& node_name,
                                     const eastl::vector<BlendSpace1DPoint>& points,
                                     float scalar, void* userdata);
  void visitBlendSpaces(BlendSpaceVisitor visitor, void* userdata) const;

  using BlendSpace2DVisitor = void (*)(
      const eastl::string& node_name,
      const eastl::vector<BlendSpace2DPoint>& points, float x, float y,
      void* userdata);
  void visitBlendSpaces2D(BlendSpace2DVisitor visitor, void* userdata) const;

  using StateVisitor = void (*)(const eastl::string& state_name,
                                AnimationStatePlaybackKind kind,
                                const eastl::string& clip_name,
                                const eastl::string& blend_space_node,
                                void* userdata);
  void visitStates(StateVisitor visitor, void* userdata) const;

  void advance(float delta_seconds);

  void sampleOntoSkeleton(Skeleton& skeleton);
  void sampleBoundSkeleton();

  /// Resolve a clip logical name to its GUID via the co-located AnimationPlayer map.
  bool resolveClipGuid(const eastl::string& name, eastl::string& out_guid) const;

  /// Resolve clip data for a mapped logical name (injected clips or resolver).
  bool resolveClipForName(const eastl::string& name,
                          AnimationClipData& out_clip) const;

 private:
  void syncPlayerSamplingBlock();
  void syncPlayerPlaybackClock();
  void notifyPlayerPoseApplied();
  float getDominantBasePlaybackPosition() const;
  float getDominantBaseClipLength() const;
  bool resolveDominantBaseClip(AnimationClipData& out_clip) const;
  void dispatchDominantMethodKeysCrossed(float prev_time, float new_time);
  void resetMethodDispatchClock(float clock = 0.0f);
  void sampleBaseOntoSkeleton(Skeleton& skeleton);
  bool applyStatePlayback(const AnimationStateDefinition& state);
  void evaluateTransitions();
  bool evaluateTransitionCondition(const StateMachineTransition& edge) const;
  bool resolveConditionFloat(const StateMachineTransition& edge,
                             float& out_value) const;
  bool resolveConditionBool(const StateMachineTransition& edge,
                            bool& out_value) const;
  bool sampleBlendSpace1DOntoSkeleton(Skeleton& skeleton,
                                    const eastl::string& node_name,
                                    float scalar);
  bool sampleBlendSpace2DOntoSkeleton(Skeleton& skeleton,
                                      const eastl::string& node_name, float x,
                                      float y);
  bool resolveDominantBlendSpace2DClip(const eastl::string& node_name, float x,
                                       float y,
                                       AnimationClipData& out_clip) const;

  AnimationPlayer* m_animation_player{nullptr};
  Skeleton* m_sampling_skeleton{nullptr};
  SkeletonModifierChainFn m_skeleton_modifier_chain_fn{nullptr};
  void* m_skeleton_modifier_chain_userdata{nullptr};
  bool m_active{false};
  eastl::string m_sample_clip_name;
  float m_sample_time{0.0f};
  eastl::string m_add2_clip_name;
  float m_add2_weight{0.0f};
  float m_add2_time{0.0f};
  eastl::hash_map<eastl::string, eastl::vector<BlendSpace1DPoint>> m_blend_spaces;
  eastl::hash_map<eastl::string, float> m_blend_space_scalars;
  eastl::string m_base_blend_space_node;
  eastl::hash_map<eastl::string, eastl::vector<BlendSpace2DPoint>>
      m_blend_spaces_2d;
  eastl::hash_map<eastl::string, BlendSpace2DParam> m_blend_space_2d_params;
  eastl::string m_base_blend_space_2d_node;
  eastl::hash_map<eastl::string, AnimationStateDefinition> m_states;
  eastl::string m_current_state_name;
  struct TreeParam {
    enum class Kind { Bool, Float } kind{Kind::Float};
    bool bool_value{false};
    float float_value{0.0f};
  };
  eastl::hash_map<eastl::string, TreeParam> m_tree_params;
  eastl::vector<StateMachineTransition> m_transitions;
  eastl::hash_map<eastl::string, BlendSpace2DParam> m_canvas_layout;
  bool m_oneshot_active{false};
  eastl::string m_oneshot_clip_name;
  float m_oneshot_time{0.0f};
  eastl::string m_oneshot_slot_clip;
  bool m_clip_play_active{false};
  eastl::string m_clip_play_clip_name;
  float m_clip_play_time{0.0f};
  float m_method_prev_clock{0.0f};
  eastl::string m_asset_guid;
};

}  // namespace Blunder
