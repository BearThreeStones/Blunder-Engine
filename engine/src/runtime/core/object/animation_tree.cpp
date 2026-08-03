#include "runtime/core/object/animation_tree.h"

#include <cfloat>

#include "runtime/core/object/animation_player.h"
#include "runtime/core/object/animation_sampler.h"
#include "runtime/core/object/skeleton.h"

namespace Blunder {

namespace {

struct BlendSpaceNeighbor {
  const BlendSpace1DPoint* left{nullptr};
  const BlendSpace1DPoint* right{nullptr};
  float blend_weight{0.0f};
};

BlendSpaceNeighbor findBlendSpaceNeighbors(
    const eastl::vector<BlendSpace1DPoint>& points, float scalar) {
  BlendSpaceNeighbor result;
  if (points.empty()) {
    return result;
  }

  const BlendSpace1DPoint* left = nullptr;
  const BlendSpace1DPoint* right = nullptr;
  float left_scalar = -FLT_MAX;
  float right_scalar = FLT_MAX;

  for (const BlendSpace1DPoint& point : points) {
    if (point.scalar <= scalar && point.scalar >= left_scalar) {
      left = &point;
      left_scalar = point.scalar;
    }
    if (point.scalar >= scalar && point.scalar <= right_scalar) {
      right = &point;
      right_scalar = point.scalar;
    }
  }

  if (left == nullptr) {
    left = right = &points[0];
    for (const BlendSpace1DPoint& point : points) {
      if (point.scalar < left->scalar) {
        left = right = &point;
      }
    }
    result.left = left;
    result.right = right;
    result.blend_weight = 0.0f;
    return result;
  }
  if (right == nullptr) {
    left = right = &points[0];
    for (const BlendSpace1DPoint& point : points) {
      if (point.scalar > left->scalar) {
        left = right = &point;
      }
    }
    result.left = left;
    result.right = right;
    result.blend_weight = 0.0f;
    return result;
  }

  result.left = left;
  result.right = right;
  if (left == right) {
    result.blend_weight = 0.0f;
  } else {
    const float span = right->scalar - left->scalar;
    result.blend_weight =
        span > 0.0f ? (scalar - left->scalar) / span : 0.0f;
  }
  return result;
}

}  // namespace

void AnimationTree::bindAnimationPlayer(AnimationPlayer* player) {
  m_animation_player = player;
  syncPlayerSamplingBlock();
}

void AnimationTree::bindSamplingSkeleton(Skeleton* skeleton) {
  m_sampling_skeleton = skeleton;
}

bool AnimationTree::setActive(bool active) {
  m_active = active;
  syncPlayerSamplingBlock();
  if (m_active) {
    sampleBoundSkeleton();
  } else if (m_animation_player != nullptr) {
    m_animation_player->resampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::setSampleClipName(const eastl::string& name) {
  if (name.empty()) {
    m_sample_clip_name.clear();
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(name, guid)) {
    return false;
  }
  m_sample_clip_name = name;
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::setAdd2ClipName(const eastl::string& name) {
  if (name.empty()) {
    m_add2_clip_name.clear();
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(name, guid)) {
    return false;
  }
  m_add2_clip_name = name;
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

void AnimationTree::setAdd2Weight(float weight) {
  m_add2_weight = weight;
  if (m_active) {
    sampleBoundSkeleton();
  }
}

bool AnimationTree::addBlendSpacePoint(const eastl::string& node_name,
                                     const eastl::string& clip_name,
                                     float scalar) {
  if (node_name.empty() || clip_name.empty()) {
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(clip_name, guid)) {
    return false;
  }
  m_blend_spaces[node_name].push_back({clip_name, scalar});
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

void AnimationTree::clearBlendSpacePoints(const eastl::string& node_name) {
  m_blend_spaces.erase(node_name);
  if (m_active) {
    sampleBoundSkeleton();
  }
}

void AnimationTree::setBlendSpaceScalar(const eastl::string& node_name,
                                        float scalar) {
  m_blend_space_scalars[node_name] = scalar;
  if (m_active) {
    sampleBoundSkeleton();
  }
}

float AnimationTree::getBlendSpaceScalar(
    const eastl::string& node_name) const {
  const auto it = m_blend_space_scalars.find(node_name);
  if (it == m_blend_space_scalars.end()) {
    return 0.0f;
  }
  return it->second;
}

bool AnimationTree::setBaseBlendSpaceNode(const eastl::string& node_name) {
  if (node_name.empty()) {
    m_base_blend_space_node.clear();
    return false;
  }
  const auto it = m_blend_spaces.find(node_name);
  if (it == m_blend_spaces.end() || it->second.empty()) {
    return false;
  }
  m_base_blend_space_node = node_name;
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::setStateClip(const eastl::string& state_name,
                               const eastl::string& clip_name) {
  if (state_name.empty() || clip_name.empty()) {
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(clip_name, guid)) {
    return false;
  }
  AnimationStateDefinition state;
  state.kind = AnimationStatePlaybackKind::Clip;
  state.clip_name = clip_name;
  m_states[state_name] = state;
  return true;
}

bool AnimationTree::setStateBlendSpace(const eastl::string& state_name,
                                       const eastl::string& blend_space_node) {
  if (state_name.empty() || blend_space_node.empty()) {
    return false;
  }
  const auto it = m_blend_spaces.find(blend_space_node);
  if (it == m_blend_spaces.end() || it->second.empty()) {
    return false;
  }
  AnimationStateDefinition state;
  state.kind = AnimationStatePlaybackKind::BlendSpace1D;
  state.blend_space_node = blend_space_node;
  m_states[state_name] = state;
  return true;
}

bool AnimationTree::applyStatePlayback(const AnimationStateDefinition& state) {
  if (state.kind == AnimationStatePlaybackKind::BlendSpace1D) {
    m_base_blend_space_node = state.blend_space_node;
    m_sample_clip_name.clear();
    return true;
  }
  m_base_blend_space_node.clear();
  m_sample_clip_name = state.clip_name;
  return true;
}

bool AnimationTree::travel(const eastl::string& state_name) {
  const auto it = m_states.find(state_name);
  if (it == m_states.end()) {
    return false;
  }
  if (!applyStatePlayback(it->second)) {
    return false;
  }
  m_current_state_name = state_name;
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::start(const eastl::string& state_name) {
  if (!travel(state_name)) {
    return false;
  }
  m_sample_time = 0.0f;
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::requestOneShot(const eastl::string& clip_name) {
  if (clip_name.empty()) {
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(clip_name, guid)) {
    return false;
  }
  m_oneshot_clip_name = clip_name;
  m_oneshot_time = 0.0f;
  m_oneshot_active = true;
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

void AnimationTree::advance(float delta_seconds) {
  if (delta_seconds <= 0.0f) {
    return;
  }

  float time_scale = 1.0f;
  if (m_animation_player != nullptr) {
    time_scale = m_animation_player->getTimeScale();
  }
  const float scaled_delta = delta_seconds * time_scale;
  if (scaled_delta <= 0.0f) {
    return;
  }

  m_sample_time += scaled_delta;

  if (m_oneshot_active) {
    m_oneshot_time += scaled_delta;
    AnimationClipData clip;
    if (resolveClipForName(m_oneshot_clip_name, clip) &&
        m_oneshot_time >= clip.duration) {
      m_oneshot_active = false;
      m_oneshot_clip_name.clear();
      m_oneshot_time = 0.0f;
    } else if (!resolveClipForName(m_oneshot_clip_name, clip)) {
      m_oneshot_active = false;
      m_oneshot_clip_name.clear();
      m_oneshot_time = 0.0f;
    }
  }

  if (m_active) {
    sampleBoundSkeleton();
  }
}

bool AnimationTree::sampleBlendSpace1DOntoSkeleton(
    Skeleton& skeleton, const eastl::string& node_name, float scalar) {
  const auto space_it = m_blend_spaces.find(node_name);
  if (space_it == m_blend_spaces.end() || space_it->second.empty()) {
    return false;
  }

  const BlendSpaceNeighbor neighbors =
      findBlendSpaceNeighbors(space_it->second, scalar);
  if (neighbors.left == nullptr || neighbors.right == nullptr) {
    return false;
  }

  AnimationClipData left_clip;
  if (!resolveClipForName(neighbors.left->clip_name, left_clip)) {
    return false;
  }

  if (neighbors.left == neighbors.right) {
    sampleClipOntoSkeleton(skeleton, left_clip, m_sample_time);
    return true;
  }

  AnimationClipData right_clip;
  if (!resolveClipForName(neighbors.right->clip_name, right_clip)) {
    return false;
  }

  blendClipsOntoSkeleton(skeleton, left_clip, m_sample_time, right_clip,
                         m_sample_time, neighbors.blend_weight);
  return true;
}

void AnimationTree::sampleBaseOntoSkeleton(Skeleton& skeleton) {
  if (m_oneshot_active && !m_oneshot_clip_name.empty()) {
    AnimationClipData clip;
    if (resolveClipForName(m_oneshot_clip_name, clip)) {
      sampleClipOntoSkeleton(skeleton, clip, m_oneshot_time);
      return;
    }
  }

  if (!m_base_blend_space_node.empty()) {
    const float scalar = getBlendSpaceScalar(m_base_blend_space_node);
    if (sampleBlendSpace1DOntoSkeleton(skeleton, m_base_blend_space_node,
                                       scalar)) {
      return;
    }
  }

  if (m_sample_clip_name.empty()) {
    return;
  }
  AnimationClipData clip;
  if (!resolveClipForName(m_sample_clip_name, clip)) {
    return;
  }
  sampleClipOntoSkeleton(skeleton, clip, m_sample_time);
}

void AnimationTree::sampleOntoSkeleton(Skeleton& skeleton) {
  if (!m_active) {
    return;
  }

  const bool has_base = !m_base_blend_space_node.empty() ||
                        !m_sample_clip_name.empty();
  if (!has_base) {
    return;
  }

  sampleBaseOntoSkeleton(skeleton);

  if (m_add2_weight > 0.0f && !m_add2_clip_name.empty()) {
    AnimationClipData add2_clip;
    if (resolveClipForName(m_add2_clip_name, add2_clip)) {
      applyAdditiveClipOntoSkeleton(skeleton, add2_clip, m_add2_time,
                                    m_add2_weight);
    }
  }
}

void AnimationTree::sampleBoundSkeleton() {
  if (m_sampling_skeleton != nullptr && m_active) {
    sampleOntoSkeleton(*m_sampling_skeleton);
    syncPlayerPlaybackClock();
    notifyPlayerPoseApplied();
  }
}

void AnimationTree::syncPlayerPlaybackClock() {
  if (m_animation_player == nullptr || !m_active) {
    return;
  }
  m_animation_player->syncTreePlaybackClock(getDominantBasePlaybackPosition(),
                                            getDominantBaseClipLength());
}

void AnimationTree::notifyPlayerPoseApplied() {
  if (m_animation_player == nullptr || !m_active) {
    return;
  }
  m_animation_player->notifyPoseAppliedFromTree();
}

float AnimationTree::getDominantBasePlaybackPosition() const {
  if (m_oneshot_active) {
    return m_oneshot_time;
  }
  return m_sample_time;
}

float AnimationTree::getDominantBaseClipLength() const {
  if (m_oneshot_active && !m_oneshot_clip_name.empty()) {
    AnimationClipData clip;
    if (resolveClipForName(m_oneshot_clip_name, clip)) {
      return clip.duration;
    }
    return 0.0f;
  }

  if (!m_base_blend_space_node.empty()) {
    const auto space_it = m_blend_spaces.find(m_base_blend_space_node);
    if (space_it != m_blend_spaces.end() && !space_it->second.empty()) {
      const float scalar = getBlendSpaceScalar(m_base_blend_space_node);
      const BlendSpaceNeighbor neighbors =
          findBlendSpaceNeighbors(space_it->second, scalar);
      if (neighbors.left != nullptr) {
        const BlendSpace1DPoint* dominant = neighbors.left;
        if (neighbors.right != nullptr && neighbors.left != neighbors.right &&
            neighbors.blend_weight > 0.5f) {
          dominant = neighbors.right;
        }
        AnimationClipData clip;
        if (resolveClipForName(dominant->clip_name, clip)) {
          return clip.duration;
        }
      }
    }
  }

  if (!m_sample_clip_name.empty()) {
    AnimationClipData clip;
    if (resolveClipForName(m_sample_clip_name, clip)) {
      return clip.duration;
    }
  }

  return 0.0f;
}

void AnimationTree::syncPlayerSamplingBlock() {
  if (m_animation_player != nullptr) {
    m_animation_player->setTreeBlocksSampling(m_active);
  }
}

bool AnimationTree::resolveClipGuid(const eastl::string& name,
                                    eastl::string& out_guid) const {
  if (m_animation_player == nullptr) {
    return false;
  }
  return m_animation_player->getClipGuid(name, out_guid);
}

bool AnimationTree::resolveClipForName(const eastl::string& name,
                                     AnimationClipData& out_clip) const {
  if (m_animation_player == nullptr) {
    return false;
  }
  return m_animation_player->resolveClipForName(name, out_clip);
}

}  // namespace Blunder
