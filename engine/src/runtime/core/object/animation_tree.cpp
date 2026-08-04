#include "runtime/core/object/animation_tree.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>

#include "runtime/core/object/animation_method_dispatch.h"
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

struct BlendSpace2DTriangle {
  int i0{0};
  int i1{0};
  int i2{0};
};

struct BlendSpace2DBlend {
  int i0{0};
  int i1{0};
  int i2{0};
  float w0{1.0f};
  float w1{0.0f};
  float w2{0.0f};
  int point_count{1};
};

float orient2d(float ax, float ay, float bx, float by, float cx, float cy) {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

bool barycentricWeights(float px, float py, float ax, float ay, float bx,
                        float by, float cx, float cy, float& w0, float& w1,
                        float& w2) {
  const float area = orient2d(ax, ay, bx, by, cx, cy);
  if (std::fabs(area) <= 1.0e-8f) {
    return false;
  }
  w0 = orient2d(px, py, bx, by, cx, cy) / area;
  w1 = orient2d(ax, ay, px, py, cx, cy) / area;
  w2 = orient2d(ax, ay, bx, by, px, py) / area;
  constexpr float kEps = -1.0e-4f;
  return w0 >= kEps && w1 >= kEps && w2 >= kEps;
}

bool inCircumcircle(float px, float py, float ax, float ay, float bx, float by,
                    float cx, float cy) {
  const float adx = ax - px;
  const float ady = ay - py;
  const float bdx = bx - px;
  const float bdy = by - py;
  const float cdx = cx - px;
  const float cdy = cy - py;

  const float abdet = adx * bdy - bdx * ady;
  const float bcdet = bdx * cdy - cdx * bdy;
  const float cadet = cdx * ady - adx * cdy;
  const float alift = adx * adx + ady * ady;
  const float blift = bdx * bdx + bdy * bdy;
  const float clift = cdx * cdx + cdy * cdy;

  const float det = alift * bcdet + blift * cadet + clift * abdet;
  const float orient = orient2d(ax, ay, bx, by, cx, cy);
  return orient * det > 0.0f;
}

eastl::vector<BlendSpace2DTriangle> buildDelaunayTriangles(
    const eastl::vector<BlendSpace2DPoint>& points) {
  eastl::vector<BlendSpace2DTriangle> triangles;
  if (points.size() < 3) {
    return triangles;
  }

  float min_x = points[0].x;
  float max_x = points[0].x;
  float min_y = points[0].y;
  float max_y = points[0].y;
  for (const BlendSpace2DPoint& point : points) {
    min_x = std::min(min_x, point.x);
    max_x = std::max(max_x, point.x);
    min_y = std::min(min_y, point.y);
    max_y = std::max(max_y, point.y);
  }

  const float dx = std::max(max_x - min_x, 1.0f);
  const float dy = std::max(max_y - min_y, 1.0f);
  const float delta = std::max(dx, dy) * 10.0f;
  const float mid_x = 0.5f * (min_x + max_x);
  const float mid_y = 0.5f * (min_y + max_y);

  eastl::vector<BlendSpace2DPoint> verts = points;
  const int s0 = static_cast<int>(verts.size());
  verts.push_back({eastl::string(), mid_x - 2.0f * delta, mid_y - delta});
  verts.push_back({eastl::string(), mid_x + 2.0f * delta, mid_y - delta});
  verts.push_back({eastl::string(), mid_x, mid_y + 2.0f * delta});
  triangles.push_back({s0, s0 + 1, s0 + 2});

  for (int pi = 0; pi < static_cast<int>(points.size()); ++pi) {
    const float px = points[static_cast<size_t>(pi)].x;
    const float py = points[static_cast<size_t>(pi)].y;

    eastl::vector<BlendSpace2DTriangle> bad;
    for (const BlendSpace2DTriangle& tri : triangles) {
      if (inCircumcircle(px, py, verts[static_cast<size_t>(tri.i0)].x,
                         verts[static_cast<size_t>(tri.i0)].y,
                         verts[static_cast<size_t>(tri.i1)].x,
                         verts[static_cast<size_t>(tri.i1)].y,
                         verts[static_cast<size_t>(tri.i2)].x,
                         verts[static_cast<size_t>(tri.i2)].y)) {
        bad.push_back(tri);
      }
    }

    struct Edge {
      int a;
      int b;
    };
    eastl::vector<Edge> polygon;
    auto pushUniqueEdge = [&polygon](int a, int b) {
      for (size_t i = 0; i < polygon.size(); ++i) {
        if ((polygon[i].a == a && polygon[i].b == b) ||
            (polygon[i].a == b && polygon[i].b == a)) {
          polygon.erase(polygon.begin() + static_cast<ptrdiff_t>(i));
          return;
        }
      }
      polygon.push_back({a, b});
    };

    for (const BlendSpace2DTriangle& tri : bad) {
      pushUniqueEdge(tri.i0, tri.i1);
      pushUniqueEdge(tri.i1, tri.i2);
      pushUniqueEdge(tri.i2, tri.i0);
    }

    eastl::vector<BlendSpace2DTriangle> next;
    next.reserve(triangles.size());
    for (const BlendSpace2DTriangle& tri : triangles) {
      bool is_bad = false;
      for (const BlendSpace2DTriangle& bad_tri : bad) {
        if (tri.i0 == bad_tri.i0 && tri.i1 == bad_tri.i1 &&
            tri.i2 == bad_tri.i2) {
          is_bad = true;
          break;
        }
      }
      if (!is_bad) {
        next.push_back(tri);
      }
    }
    for (const Edge& edge : polygon) {
      next.push_back({edge.a, edge.b, pi});
    }
    triangles.swap(next);
  }

  eastl::vector<BlendSpace2DTriangle> filtered;
  for (const BlendSpace2DTriangle& tri : triangles) {
    if (tri.i0 >= s0 || tri.i1 >= s0 || tri.i2 >= s0) {
      continue;
    }
    if (std::fabs(orient2d(points[static_cast<size_t>(tri.i0)].x,
                           points[static_cast<size_t>(tri.i0)].y,
                           points[static_cast<size_t>(tri.i1)].x,
                           points[static_cast<size_t>(tri.i1)].y,
                           points[static_cast<size_t>(tri.i2)].x,
                           points[static_cast<size_t>(tri.i2)].y)) <=
        1.0e-8f) {
      continue;
    }
    filtered.push_back(tri);
  }
  return filtered;
}

BlendSpace2DBlend resolveBlendSpace2D(const eastl::vector<BlendSpace2DPoint>& points,
                                      float x, float y) {
  BlendSpace2DBlend result;
  if (points.empty()) {
    result.point_count = 0;
    return result;
  }
  if (points.size() == 1) {
    result.i0 = 0;
    result.w0 = 1.0f;
    result.point_count = 1;
    return result;
  }
  if (points.size() == 2) {
    const float ax = points[0].x;
    const float ay = points[0].y;
    const float bx = points[1].x;
    const float by = points[1].y;
    const float abx = bx - ax;
    const float aby = by - ay;
    const float denom = abx * abx + aby * aby;
    float t = 0.0f;
    if (denom > 1.0e-8f) {
      t = ((x - ax) * abx + (y - ay) * aby) / denom;
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
    }
    result.i0 = 0;
    result.i1 = 1;
    result.w0 = 1.0f - t;
    result.w1 = t;
    result.point_count = 2;
    return result;
  }

  const eastl::vector<BlendSpace2DTriangle> triangles =
      buildDelaunayTriangles(points);
  for (const BlendSpace2DTriangle& tri : triangles) {
    float w0 = 0.0f;
    float w1 = 0.0f;
    float w2 = 0.0f;
    if (barycentricWeights(x, y, points[static_cast<size_t>(tri.i0)].x,
                           points[static_cast<size_t>(tri.i0)].y,
                           points[static_cast<size_t>(tri.i1)].x,
                           points[static_cast<size_t>(tri.i1)].y,
                           points[static_cast<size_t>(tri.i2)].x,
                           points[static_cast<size_t>(tri.i2)].y, w0, w1,
                           w2)) {
      result.i0 = tri.i0;
      result.i1 = tri.i1;
      result.i2 = tri.i2;
      result.w0 = w0;
      result.w1 = w1;
      result.w2 = w2;
      result.point_count = 3;
      return result;
    }
  }

  // Outside triangulation: nearest authored point (stable tie-break by index).
  int nearest = 0;
  float best_dist2 = FLT_MAX;
  for (int i = 0; i < static_cast<int>(points.size()); ++i) {
    const float dx = points[static_cast<size_t>(i)].x - x;
    const float dy = points[static_cast<size_t>(i)].y - y;
    const float dist2 = dx * dx + dy * dy;
    if (dist2 < best_dist2) {
      best_dist2 = dist2;
      nearest = i;
    }
  }
  result.i0 = nearest;
  result.w0 = 1.0f;
  result.point_count = 1;
  return result;
}

int dominantBlendSpace2DIndex(const BlendSpace2DBlend& blend) {
  if (blend.point_count <= 1) {
    return blend.i0;
  }
  if (blend.point_count == 2) {
    return blend.w1 > blend.w0 ? blend.i1 : blend.i0;
  }

  struct Candidate {
    int index;
    float weight;
  };
  Candidate c[3] = {{blend.i0, blend.w0}, {blend.i1, blend.w1}, {blend.i2, blend.w2}};
  Candidate best = c[0];
  for (int i = 1; i < 3; ++i) {
    if (c[i].weight > best.weight + 1.0e-5f ||
        (std::fabs(c[i].weight - best.weight) <= 1.0e-5f &&
         c[i].index < best.index)) {
      best = c[i];
    }
  }
  return best.index;
}

}  // namespace

void AnimationTree::bindAnimationPlayer(AnimationPlayer* player) {
  if (m_animation_player != nullptr) {
    m_animation_player->bindAnimationTree(nullptr);
  }
  m_animation_player = player;
  if (m_animation_player != nullptr) {
    m_animation_player->bindAnimationTree(this);
  }
  syncPlayerSamplingBlock();
}

void AnimationTree::bindSamplingSkeleton(Skeleton* skeleton) {
  m_sampling_skeleton = skeleton;
}

void AnimationTree::bindSkeletonModifierChain(SkeletonModifierChainFn fn,
                                              void* userdata) {
  m_skeleton_modifier_chain_fn = fn;
  m_skeleton_modifier_chain_userdata = userdata;
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
  m_base_blend_space_2d_node.clear();
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::addBlendSpace2DPoint(const eastl::string& node_name,
                                         const eastl::string& clip_name,
                                         float x, float y) {
  if (node_name.empty() || clip_name.empty()) {
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(clip_name, guid)) {
    return false;
  }
  m_blend_spaces_2d[node_name].push_back({clip_name, x, y});
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

void AnimationTree::clearBlendSpace2DPoints(const eastl::string& node_name) {
  m_blend_spaces_2d.erase(node_name);
  if (m_active) {
    sampleBoundSkeleton();
  }
}

void AnimationTree::setBlendSpace2DParam(const eastl::string& node_name, float x,
                                         float y) {
  m_blend_space_2d_params[node_name] = {x, y};
  if (m_active) {
    sampleBoundSkeleton();
  }
}

BlendSpace2DParam AnimationTree::getBlendSpace2DParam(
    const eastl::string& node_name) const {
  const auto it = m_blend_space_2d_params.find(node_name);
  if (it == m_blend_space_2d_params.end()) {
    return {};
  }
  return it->second;
}

bool AnimationTree::setBaseBlendSpace2DNode(const eastl::string& node_name) {
  if (node_name.empty()) {
    m_base_blend_space_2d_node.clear();
    return false;
  }
  const auto it = m_blend_spaces_2d.find(node_name);
  if (it == m_blend_spaces_2d.end() || it->second.empty()) {
    return false;
  }
  m_base_blend_space_2d_node = node_name;
  m_base_blend_space_node.clear();
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

bool AnimationTree::setStateBlendSpace2D(const eastl::string& state_name,
                                         const eastl::string& blend_space_node) {
  if (state_name.empty() || blend_space_node.empty()) {
    return false;
  }
  const auto it = m_blend_spaces_2d.find(blend_space_node);
  if (it == m_blend_spaces_2d.end() || it->second.empty()) {
    return false;
  }
  AnimationStateDefinition state;
  state.kind = AnimationStatePlaybackKind::BlendSpace2D;
  state.blend_space_node = blend_space_node;
  m_states[state_name] = state;
  return true;
}

bool AnimationTree::applyStatePlayback(const AnimationStateDefinition& state) {
  if (state.kind == AnimationStatePlaybackKind::BlendSpace1D) {
    m_base_blend_space_node = state.blend_space_node;
    m_base_blend_space_2d_node.clear();
    m_sample_clip_name.clear();
    return true;
  }
  if (state.kind == AnimationStatePlaybackKind::BlendSpace2D) {
    m_base_blend_space_2d_node = state.blend_space_node;
    m_base_blend_space_node.clear();
    m_sample_clip_name.clear();
    return true;
  }
  m_base_blend_space_node.clear();
  m_base_blend_space_2d_node.clear();
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
  resetMethodDispatchClock(0.0f);
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
  resetMethodDispatchClock(0.0f);
  if (m_active) {
    sampleBoundSkeleton();
  }
  return true;
}

bool AnimationTree::setOneShotSlotClip(const eastl::string& clip_name) {
  if (clip_name.empty()) {
    m_oneshot_slot_clip.clear();
    return false;
  }
  eastl::string guid;
  if (!resolveClipGuid(clip_name, guid)) {
    return false;
  }
  m_oneshot_slot_clip = clip_name;
  return true;
}

void AnimationTree::visitBlendSpaces(BlendSpaceVisitor visitor,
                                     void* userdata) const {
  if (visitor == nullptr) {
    return;
  }
  for (const auto& entry : m_blend_spaces) {
    const float scalar = getBlendSpaceScalar(entry.first);
    visitor(entry.first, entry.second, scalar, userdata);
  }
}

void AnimationTree::visitStates(StateVisitor visitor, void* userdata) const {
  if (visitor == nullptr) {
    return;
  }
  for (const auto& entry : m_states) {
    const AnimationStateDefinition& state = entry.second;
    visitor(entry.first, state.kind, state.clip_name, state.blend_space_node,
            userdata);
  }
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

  const float prev_clock = getDominantBasePlaybackPosition();
  const bool was_oneshot = m_oneshot_active;
  eastl::string ended_oneshot_clip_name;
  float ended_oneshot_duration = 0.0f;
  if (was_oneshot) {
    ended_oneshot_clip_name = m_oneshot_clip_name;
    AnimationClipData oneshot_clip;
    if (resolveClipForName(ended_oneshot_clip_name, oneshot_clip)) {
      ended_oneshot_duration = oneshot_clip.duration;
    }
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

  const float new_clock = getDominantBasePlaybackPosition();
  const bool oneshot_ended = was_oneshot && !m_oneshot_active;

  if (was_oneshot && m_oneshot_active) {
    dispatchDominantMethodKeysCrossed(prev_clock, new_clock);
    resetMethodDispatchClock(new_clock);
  } else if (oneshot_ended && !ended_oneshot_clip_name.empty() &&
             ended_oneshot_duration > 0.0f &&
             m_animation_player != nullptr &&
             isValid(m_animation_player->getOwnerObjectId())) {
    AnimationClipData oneshot_clip;
    if (resolveClipForName(ended_oneshot_clip_name, oneshot_clip)) {
      const bool looping =
          m_animation_player != nullptr && m_animation_player->isLooping();
      dispatchAnimationMethodKeysCrossed(m_animation_player->getOwnerObjectId(),
                                         oneshot_clip, prev_clock,
                                         ended_oneshot_duration, looping);
    }
    resetMethodDispatchClock(new_clock);
  } else if (!was_oneshot) {
    dispatchDominantMethodKeysCrossed(prev_clock, new_clock);
    resetMethodDispatchClock(new_clock);
  } else {
    resetMethodDispatchClock(new_clock);
  }

  if (m_active) {
    sampleBoundSkeleton();
  }
}

void AnimationTree::resetMethodDispatchClock(float clock) {
  m_method_prev_clock = clock;
}

void AnimationTree::dispatchDominantMethodKeysCrossed(float prev_time,
                                                    float new_time) {
  if (m_animation_player == nullptr) {
    return;
  }
  const ObjectId owner_id = m_animation_player->getOwnerObjectId();
  if (!isValid(owner_id)) {
    return;
  }

  AnimationClipData clip;
  if (!resolveDominantBaseClip(clip)) {
    return;
  }

  const bool looping =
      m_animation_player != nullptr && m_animation_player->isLooping();
  dispatchAnimationMethodKeysCrossed(owner_id, clip, prev_time, new_time,
                                     looping);
}

bool AnimationTree::resolveDominantBaseClip(AnimationClipData& out_clip) const {
  if (m_oneshot_active && !m_oneshot_clip_name.empty()) {
    return resolveClipForName(m_oneshot_clip_name, out_clip);
  }

  if (!m_base_blend_space_2d_node.empty()) {
    const BlendSpace2DParam param =
        getBlendSpace2DParam(m_base_blend_space_2d_node);
    return resolveDominantBlendSpace2DClip(m_base_blend_space_2d_node, param.x,
                                           param.y, out_clip);
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
        return resolveClipForName(dominant->clip_name, out_clip);
      }
    }
  }

  if (!m_sample_clip_name.empty()) {
    return resolveClipForName(m_sample_clip_name, out_clip);
  }

  return false;
}

bool AnimationTree::resolveDominantBlendSpace2DClip(
    const eastl::string& node_name, float x, float y,
    AnimationClipData& out_clip) const {
  const auto space_it = m_blend_spaces_2d.find(node_name);
  if (space_it == m_blend_spaces_2d.end() || space_it->second.empty()) {
    return false;
  }
  const BlendSpace2DBlend blend = resolveBlendSpace2D(space_it->second, x, y);
  if (blend.point_count <= 0) {
    return false;
  }
  const int dominant = dominantBlendSpace2DIndex(blend);
  return resolveClipForName(
      space_it->second[static_cast<size_t>(dominant)].clip_name, out_clip);
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

bool AnimationTree::sampleBlendSpace2DOntoSkeleton(
    Skeleton& skeleton, const eastl::string& node_name, float x, float y) {
  const auto space_it = m_blend_spaces_2d.find(node_name);
  if (space_it == m_blend_spaces_2d.end() || space_it->second.empty()) {
    return false;
  }

  const BlendSpace2DBlend blend = resolveBlendSpace2D(space_it->second, x, y);
  if (blend.point_count <= 0) {
    return false;
  }

  AnimationClipData clip0;
  if (!resolveClipForName(
          space_it->second[static_cast<size_t>(blend.i0)].clip_name, clip0)) {
    return false;
  }

  if (blend.point_count == 1) {
    sampleClipOntoSkeleton(skeleton, clip0, m_sample_time);
    return true;
  }

  AnimationClipData clip1;
  if (!resolveClipForName(
          space_it->second[static_cast<size_t>(blend.i1)].clip_name, clip1)) {
    return false;
  }

  if (blend.point_count == 2) {
    blendClipsOntoSkeleton(skeleton, clip0, m_sample_time, clip1, m_sample_time,
                           blend.w1);
    return true;
  }

  AnimationClipData clip2;
  if (!resolveClipForName(
          space_it->second[static_cast<size_t>(blend.i2)].clip_name, clip2)) {
    return false;
  }

  blendThreeClipsOntoSkeleton(skeleton, clip0, m_sample_time, blend.w0, clip1,
                              m_sample_time, blend.w1, clip2, m_sample_time,
                              blend.w2);
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

  if (!m_base_blend_space_2d_node.empty()) {
    const BlendSpace2DParam param =
        getBlendSpace2DParam(m_base_blend_space_2d_node);
    if (sampleBlendSpace2DOntoSkeleton(skeleton, m_base_blend_space_2d_node,
                                       param.x, param.y)) {
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
                        !m_base_blend_space_2d_node.empty() ||
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
    if (m_skeleton_modifier_chain_fn != nullptr) {
      m_skeleton_modifier_chain_fn(*m_sampling_skeleton,
                                   m_skeleton_modifier_chain_userdata);
    }
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

  if (!m_base_blend_space_2d_node.empty()) {
    AnimationClipData clip;
    const BlendSpace2DParam param =
        getBlendSpace2DParam(m_base_blend_space_2d_node);
    if (resolveDominantBlendSpace2DClip(m_base_blend_space_2d_node, param.x,
                                        param.y, clip)) {
      return clip.duration;
    }
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
