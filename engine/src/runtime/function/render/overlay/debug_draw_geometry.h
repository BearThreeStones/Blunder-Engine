#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/overlay/overlay_state.h"

namespace Blunder {

constexpr int k_debug_draw_min_circle_steps = 8;
constexpr int k_debug_draw_max_circle_steps = 64;

inline int debugDrawCircleSteps(float radius, float distance,
                                float viewport_height, float vertical_fov,
                                float ortho_size, bool is_perspective) {
  const float r = std::max(radius, 1e-4f);
  const float dist = std::max(distance, 1e-3f);
  const float vp_h = std::max(viewport_height, 1.0f);
  float world_per_pixel = 0.01f;
  if (is_perspective) {
    const float world_height =
        2.0f * dist * std::tan(std::max(vertical_fov, 1e-4f) * 0.5f);
    world_per_pixel = world_height / vp_h;
  } else {
    world_per_pixel = std::max(ortho_size, 1e-3f) / vp_h;
  }
  world_per_pixel = std::max(world_per_pixel, 1e-8f);
  const float arg = 1.0f - world_per_pixel / r;
  if (arg <= -1.0f) {
    return k_debug_draw_min_circle_steps;
  }
  if (arg >= 1.0f) {
    return k_debug_draw_max_circle_steps;
  }
  const float n = 3.14159265f / std::acos(std::clamp(arg, -1.0f, 1.0f));
  const int steps = static_cast<int>(std::ceil(n));
  return std::clamp(steps, k_debug_draw_min_circle_steps,
                    k_debug_draw_max_circle_steps);
}

inline uint32_t debugDrawGpuDrawCalls(uint32_t line_count) {
  return line_count == 0u ? 0u : 2u;
}

inline int debugDrawStepsForWorldPoint(const Vec3& point, float radius,
                                       const OverlayState& state) {
  const float distance = glm::length(point - state.camera_position);
  return debugDrawCircleSteps(
      radius, distance, static_cast<float>(state.viewport_height),
      state.vertical_fov, state.ortho_size, state.is_perspective);
}

template <typename Fn>
void debugDrawEmitCircle(const Vec3& center, const Vec3& axis, float radius,
                         int steps, const Vec4& color, float width_px,
                         Fn&& fn) {
  Vec3 n = axis;
  const float nlen = glm::length(n);
  n = nlen > 1e-6f ? n / nlen : Vec3(0.0f, 0.0f, 1.0f);
  const Vec3 helper = std::fabs(n.z) < 0.9f ? Vec3(0.0f, 0.0f, 1.0f)
                                            : Vec3(1.0f, 0.0f, 0.0f);
  const Vec3 u = glm::normalize(glm::cross(helper, n));
  const Vec3 v = glm::cross(n, u);
  const float r = std::max(radius, 1e-4f);
  const int count = std::max(steps, 3);
  Vec3 prev = center + u * r;
  for (int i = 1; i <= count; ++i) {
    const float t =
        (static_cast<float>(i) / static_cast<float>(count)) * 6.28318530718f;
    const Vec3 cur = center + (u * std::cos(t) + v * std::sin(t)) * r;
    fn(prev, cur, color, width_px);
    prev = cur;
  }
}

template <typename Fn>
void debugDrawEmitArc(const Vec3& center, const Vec3& from_dir,
                      const Vec3& to_dir, float radius, int steps,
                      const Vec4& color, float width_px, Fn&& fn) {
  const float r = std::max(radius, 1e-4f);
  Vec3 a = from_dir;
  Vec3 b = to_dir;
  const float al = glm::length(a);
  const float bl = glm::length(b);
  if (al < 1e-6f || bl < 1e-6f) {
    return;
  }
  a /= al;
  b /= bl;
  const int count = std::max(steps, 2);
  Vec3 prev = center + a * r;
  for (int i = 1; i <= count; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(count);
    Vec3 dir = a * (1.0f - t) + b * t;
    const float dlen = glm::length(dir);
    dir = dlen < 1e-6f ? a : dir / dlen;
    const Vec3 cur = center + dir * r;
    fn(prev, cur, color, width_px);
    prev = cur;
  }
}

template <typename Fn>
void debugDrawEmitWireBox(const Vec3& center, const Vec3& size,
                          const Vec4& color, float width_px, Fn&& fn) {
  const Vec3 h = size * 0.5f;
  const Vec3 c0 = center + Vec3(-h.x, -h.y, -h.z);
  const Vec3 c1 = center + Vec3(h.x, -h.y, -h.z);
  const Vec3 c2 = center + Vec3(h.x, h.y, -h.z);
  const Vec3 c3 = center + Vec3(-h.x, h.y, -h.z);
  const Vec3 c4 = center + Vec3(-h.x, -h.y, h.z);
  const Vec3 c5 = center + Vec3(h.x, -h.y, h.z);
  const Vec3 c6 = center + Vec3(h.x, h.y, h.z);
  const Vec3 c7 = center + Vec3(-h.x, h.y, h.z);
  fn(c0, c1, color, width_px);
  fn(c1, c2, color, width_px);
  fn(c2, c3, color, width_px);
  fn(c3, c0, color, width_px);
  fn(c4, c5, color, width_px);
  fn(c5, c6, color, width_px);
  fn(c6, c7, color, width_px);
  fn(c7, c4, color, width_px);
  fn(c0, c4, color, width_px);
  fn(c1, c5, color, width_px);
  fn(c2, c6, color, width_px);
  fn(c3, c7, color, width_px);
}

template <typename Fn>
void debugDrawEmitCross(const Vec3& point, float size, const Vec4& color,
                        float width_px, Fn&& fn) {
  const float h = std::max(size, 0.0f) * 0.5f;
  fn(point + Vec3(-h, 0.0f, 0.0f), point + Vec3(h, 0.0f, 0.0f), color,
     width_px);
  fn(point + Vec3(0.0f, -h, 0.0f), point + Vec3(0.0f, h, 0.0f), color,
     width_px);
  fn(point + Vec3(0.0f, 0.0f, -h), point + Vec3(0.0f, 0.0f, h), color,
     width_px);
}

template <typename Fn>
void debugDrawEmitArrow(const Vec3& from, const Vec3& to, const Vec4& color,
                        float width_px, Fn&& fn) {
  fn(from, to, color, width_px);
  const Vec3 delta = to - from;
  const float len = glm::length(delta);
  if (len < 1e-5f) {
    return;
  }
  const Vec3 dir = delta / len;
  const Vec3 helper = std::fabs(dir.z) < 0.9f ? Vec3(0.0f, 0.0f, 1.0f)
                                              : Vec3(1.0f, 0.0f, 0.0f);
  const Vec3 right = glm::normalize(glm::cross(dir, helper));
  const float head = std::min(len * 0.2f, std::max(len * 0.08f, 1e-4f));
  const float wing = head * 0.4f;
  const Vec3 base = to - dir * head;
  fn(to, base + right * wing, color, width_px);
  fn(to, base - right * wing, color, width_px);
}

template <typename Fn>
void debugDrawEmitWireSphere(const Vec3& center, float radius, int steps,
                             const Vec4& color, float width_px, Fn&& fn) {
  debugDrawEmitCircle(center, Vec3(1.0f, 0.0f, 0.0f), radius, steps, color,
                      width_px, fn);
  debugDrawEmitCircle(center, Vec3(0.0f, 1.0f, 0.0f), radius, steps, color,
                      width_px, fn);
  debugDrawEmitCircle(center, Vec3(0.0f, 0.0f, 1.0f), radius, steps, color,
                      width_px, fn);
}

template <typename Fn>
void debugDrawEmitWireCapsule(const Vec3& a, const Vec3& b, float radius,
                              int steps, const Vec4& color, float width_px,
                              Fn&& fn) {
  const float r = std::max(radius, 1e-4f);
  Vec3 axis = b - a;
  const float axis_len = glm::length(axis);
  const Vec3 n = axis_len > 1e-6f ? axis / axis_len : Vec3(0.0f, 0.0f, 1.0f);
  const Vec3 helper = std::fabs(n.z) < 0.9f ? Vec3(0.0f, 0.0f, 1.0f)
                                            : Vec3(1.0f, 0.0f, 0.0f);
  const Vec3 u = glm::normalize(glm::cross(helper, n));
  const Vec3 v = glm::cross(n, u);
  debugDrawEmitCircle(a, n, r, steps, color, width_px, fn);
  debugDrawEmitCircle(b, n, r, steps, color, width_px, fn);
  fn(a + u * r, b + u * r, color, width_px);
  fn(a - u * r, b - u * r, color, width_px);
  fn(a + v * r, b + v * r, color, width_px);
  fn(a - v * r, b - v * r, color, width_px);
  const int hemi = std::max(steps / 2, 4);
  debugDrawEmitArc(a, u, -n, r, hemi, color, width_px, fn);
  debugDrawEmitArc(a, -u, -n, r, hemi, color, width_px, fn);
  debugDrawEmitArc(a, v, -n, r, hemi, color, width_px, fn);
  debugDrawEmitArc(a, -v, -n, r, hemi, color, width_px, fn);
  debugDrawEmitArc(b, u, n, r, hemi, color, width_px, fn);
  debugDrawEmitArc(b, -u, n, r, hemi, color, width_px, fn);
  debugDrawEmitArc(b, v, n, r, hemi, color, width_px, fn);
  debugDrawEmitArc(b, -v, n, r, hemi, color, width_px, fn);
}

}  // namespace Blunder

