#pragma once

#include <cmath>

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include "runtime/core/math/math_types.h"

namespace Blunder {

constexpr int kLightGizmoRingSegments = 16;
constexpr float kLightGizmoOriginCrossHalfLen = 0.08f;
constexpr float kLightGizmoDirectionalArrowLength = 1.5f;
constexpr float kLightGizmoArrowHead = 0.2f;
constexpr float kLightGizmoAreaEmitTick = 0.25f;

enum class LightGizmoKind {
  directional,
  point,
  spot,
  area,
};

struct LightGizmoShape {
  LightGizmoKind kind{LightGizmoKind::directional};
  float range{10.0f};
  float outer_cone_degrees{45.0f};
  float width{1.0f};
  float height{1.0f};
};

template <typename Fn>
void forEachLightGizmoSegmentLocal(const LightGizmoShape& shape, Fn&& fn) {
  const float cross = kLightGizmoOriginCrossHalfLen;
  fn(Vec3(-cross, 0.0f, 0.0f), Vec3(cross, 0.0f, 0.0f));
  fn(Vec3(0.0f, -cross, 0.0f), Vec3(0.0f, cross, 0.0f));
  fn(Vec3(0.0f, 0.0f, -cross), Vec3(0.0f, 0.0f, cross));

  const auto ring = [&](float radius, int axis) {
    const float r = std::max(radius, 1e-4f);
    for (int i = 0; i < kLightGizmoRingSegments; ++i) {
      const float a0 =
          (static_cast<float>(i) / static_cast<float>(kLightGizmoRingSegments)) *
          6.28318530718f;
      const float a1 = (static_cast<float>(i + 1) /
                        static_cast<float>(kLightGizmoRingSegments)) *
                       6.28318530718f;
      const float c0 = std::cos(a0);
      const float s0 = std::sin(a0);
      const float c1 = std::cos(a1);
      const float s1 = std::sin(a1);
      Vec3 p0{};
      Vec3 p1{};
      if (axis == 0) {
        p0 = Vec3(0.0f, r * c0, r * s0);
        p1 = Vec3(0.0f, r * c1, r * s1);
      } else if (axis == 1) {
        p0 = Vec3(r * c0, 0.0f, r * s0);
        p1 = Vec3(r * c1, 0.0f, r * s1);
      } else {
        p0 = Vec3(r * c0, r * s0, 0.0f);
        p1 = Vec3(r * c1, r * s1, 0.0f);
      }
      fn(p0, p1);
    }
  };

  switch (shape.kind) {
    case LightGizmoKind::directional: {
      const Vec3 tip(0.0f, 0.0f, -kLightGizmoDirectionalArrowLength);
      fn(Vec3(0.0f), tip);
      fn(tip, Vec3(kLightGizmoArrowHead, 0.0f,
                   -kLightGizmoDirectionalArrowLength + kLightGizmoArrowHead));
      fn(tip, Vec3(-kLightGizmoArrowHead, 0.0f,
                   -kLightGizmoDirectionalArrowLength + kLightGizmoArrowHead));
      fn(tip, Vec3(0.0f, kLightGizmoArrowHead,
                   -kLightGizmoDirectionalArrowLength + kLightGizmoArrowHead));
      fn(tip, Vec3(0.0f, -kLightGizmoArrowHead,
                   -kLightGizmoDirectionalArrowLength + kLightGizmoArrowHead));
      break;
    }
    case LightGizmoKind::point: {
      ring(shape.range, 0);
      ring(shape.range, 1);
      ring(shape.range, 2);
      break;
    }
    case LightGizmoKind::spot: {
      const float range = std::max(shape.range, 1e-4f);
      const float outer =
          glm::radians(glm::clamp(shape.outer_cone_degrees, 0.0f, 89.9f));
      const float radius = range * std::tan(outer);
      const Vec3 apex(0.0f);
      for (int i = 0; i < kLightGizmoRingSegments; ++i) {
        const float a0 =
            (static_cast<float>(i) / static_cast<float>(kLightGizmoRingSegments)) *
            6.28318530718f;
        const float a1 = (static_cast<float>(i + 1) /
                          static_cast<float>(kLightGizmoRingSegments)) *
                         6.28318530718f;
        const Vec3 p0(radius * std::cos(a0), radius * std::sin(a0), -range);
        const Vec3 p1(radius * std::cos(a1), radius * std::sin(a1), -range);
        fn(p0, p1);
        if (i % 2 == 0) {
          fn(apex, p0);
        }
      }
      break;
    }
    case LightGizmoKind::area: {
      const float hx = std::max(shape.width, 1e-4f) * 0.5f;
      const float hy = std::max(shape.height, 1e-4f) * 0.5f;
      const Vec3 c0(-hx, -hy, 0.0f);
      const Vec3 c1(hx, -hy, 0.0f);
      const Vec3 c2(hx, hy, 0.0f);
      const Vec3 c3(-hx, hy, 0.0f);
      fn(c0, c1);
      fn(c1, c2);
      fn(c2, c3);
      fn(c3, c0);
      fn(Vec3(0.0f), Vec3(0.0f, 0.0f, -kLightGizmoAreaEmitTick));
      break;
    }
  }
}

}  // namespace Blunder
