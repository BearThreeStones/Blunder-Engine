#pragma once

#include <cmath>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/gltf_unit_scale.h"

namespace Blunder {

constexpr int kLightGizmoRingSegments = 16;
constexpr float kLightGizmoOriginCrossHalfLen = 0.08f;
constexpr float kLightGizmoDirectionalArrowLength = 1.5f;
constexpr float kLightGizmoArrowHead = 0.2f;
constexpr float kLightGizmoAreaEmitTick = 0.25f;
/// Unselected point wire sits on the Unique, not the attenuation volume.
constexpr float kLightGizmoPointDisplayRadius = 0.45f;
/// Unselected spot cone length (outer angle still comes from the Unique).
constexpr float kLightGizmoSpotDisplayLength = 1.5f;

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
  bool show_range{false};
};

/// Translation (and rotation for directed lights) without scale/shear so a
/// parent non-uniform scale cannot squash a sphere into stacked ellipses.
inline Mat4 makeLightGizmoWorldMatrix(const Mat4& world, LightGizmoKind kind) {
  const Vec3 t(world[3]);
  Mat4 out(1.0f);
  out[3] = Vec4(t, 1.0f);
  if (kind == LightGizmoKind::point) {
    return out;
  }

  Vec3 x(world[0]);
  Vec3 y(world[1]);
  const float xlen = glm::length(x);
  const float ylen = glm::length(y);
  if (xlen < 1e-8f || ylen < 1e-8f) {
    return out;
  }
  x /= xlen;
  y = y - x * glm::dot(y, x);
  const float ylen2 = glm::length(y);
  if (ylen2 < 1e-8f) {
    return out;
  }
  y /= ylen2;
  Vec3 z = glm::cross(x, y);
  const Vec3 z_src(world[2]);
  if (glm::dot(z, z_src) < 0.0f) {
    z = -z;
    y = -y;
  }
  out[0] = Vec4(x, 0.0f);
  out[1] = Vec4(y, 0.0f);
  out[2] = Vec4(z, 0.0f);
  return out;
}

inline Vec3 overlayGizmoWorldOrigin(const Mat4& world) {
  return Vec3(world[3]);
}

/// Drawn Sponza verts stay centimetre (nodesQ0S 0.008 is absorbed on mesh
/// nodes). Unique `getWorldMatrix` can still be metres, collapsed 0.008*metres,
/// or a stale identity while the building sits hundreds of units off the
/// origin grid. Overlay translation is Unique **local** through the parent
/// with 0.008 stripped — never Unique world (that parks icons on the origin
/// grid). Lighting/fog keep `getWorldMatrix`. Wire basis grows with mesh
/// scale only.
inline Mat4 overlayParentMeshBasis(const Mat4& parent_world) {
  const Vec3 parent_scale(glm::length(Vec3(parent_world[0])),
                          glm::length(Vec3(parent_world[1])),
                          glm::length(Vec3(parent_world[2])));
  Mat4 parent_mesh = parent_world;
  if (isGltfCentimeterUniformScale(parent_scale)) {
    const float s = parent_scale.x;
    parent_mesh[0] = Vec4(Vec3(parent_world[0]) / s, 0.0f);
    parent_mesh[1] = Vec4(Vec3(parent_world[1]) / s, 0.0f);
    parent_mesh[2] = Vec4(Vec3(parent_world[2]) / s, 0.0f);
    parent_mesh[3] = Vec4(Vec3(parent_world[3]) / s, 1.0f);
  }
  return parent_mesh;
}

inline Vec3 overlayGizmoTranslationMatchingMesh(const Vec3& unique_local,
                                                const Mat4& unique_world,
                                                const Mat4& parent_world,
                                                bool centimetre_mesh) {
  (void)unique_world;
  Vec3 t = Vec3(overlayParentMeshBasis(parent_world) * Vec4(unique_local, 1.0f));
  if (centimetre_mesh && looksLikeMeterSpaceTranslation(t)) {
    t = t / kGltfCentimeterToMeterScale;
  }
  return t;
}

inline Vec3 overlayInferUniqueLocal(const Mat4& unique_world,
                                    const Mat4& parent_world) {
  const Vec3 parent_scale(glm::length(Vec3(parent_world[0])),
                          glm::length(Vec3(parent_world[1])),
                          glm::length(Vec3(parent_world[2])));
  if (isGltfCentimeterUniformScale(parent_scale)) {
    return (Vec3(unique_world[3]) - Vec3(parent_world[3])) / parent_scale.x;
  }
  const Mat4 inv_parent = glm::inverse(parent_world);
  return Vec3(inv_parent * Vec4(Vec3(unique_world[3]), 1.0f));
}

inline Mat4 makeLightGizmoWorldMatchingMesh(const Mat4& unique_world,
                                            const Mat4& parent_world,
                                            LightGizmoKind kind,
                                            bool centimetre_mesh,
                                            const Vec3& unique_local) {
  Mat4 scaled_world = unique_world;
  scaled_world[3] =
      Vec4(overlayGizmoTranslationMatchingMesh(unique_local, unique_world,
                                               parent_world, centimetre_mesh),
           1.0f);
  const Vec3 parent_scale(glm::length(Vec3(parent_world[0])),
                          glm::length(Vec3(parent_world[1])),
                          glm::length(Vec3(parent_world[2])));
  float display = centimetre_mesh ? 1.0f / kGltfCentimeterToMeterScale : 1.0f;
  if (isGltfCentimeterUniformScale(parent_scale)) {
    display = 1.0f / parent_scale.x;
  }
  Mat4 out = makeLightGizmoWorldMatrix(scaled_world, kind);
  if (display != 1.0f) {
    out[0] = Vec4(Vec3(out[0]) * display, 0.0f);
    out[1] = Vec4(Vec3(out[1]) * display, 0.0f);
    out[2] = Vec4(Vec3(out[2]) * display, 0.0f);
  }
  return out;
}

inline Mat4 makeLightGizmoWorldMatchingMesh(const Mat4& unique_world,
                                            const Mat4& parent_world,
                                            LightGizmoKind kind,
                                            bool centimetre_mesh = false) {
  return makeLightGizmoWorldMatchingMesh(
      unique_world, parent_world, kind, centimetre_mesh,
      overlayInferUniqueLocal(unique_world, parent_world));
}

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

  const auto cone = [&](float length) {
    const float range = std::max(length, 1e-4f);
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
      ring(kLightGizmoPointDisplayRadius, 0);
      ring(kLightGizmoPointDisplayRadius, 1);
      ring(kLightGizmoPointDisplayRadius, 2);
      if (shape.show_range) {
        ring(shape.range, 0);
        ring(shape.range, 1);
        ring(shape.range, 2);
      }
      break;
    }
    case LightGizmoKind::spot: {
      cone(shape.show_range ? shape.range : kLightGizmoSpotDisplayLength);
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
