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
  }
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
