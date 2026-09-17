#pragma once

#include <cmath>

#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"

namespace Blunder {

constexpr int kCollisionGizmoRingSegments = 16;

namespace collision_gizmo_detail {

template <typename Fn>
void emitAxisRing(float radius, int axis, float axis_offset, Fn&& fn) {
  const float r = std::max(radius, 1e-4f);
  for (int i = 0; i < kCollisionGizmoRingSegments; ++i) {
    const float a0 =
        (static_cast<float>(i) / static_cast<float>(kCollisionGizmoRingSegments)) *
        6.28318530718f;
    const float a1 = (static_cast<float>(i + 1) /
                      static_cast<float>(kCollisionGizmoRingSegments)) *
                     6.28318530718f;
    const float c0 = std::cos(a0);
    const float s0 = std::sin(a0);
    const float c1 = std::cos(a1);
    const float s1 = std::sin(a1);
    Vec3 p0{};
    Vec3 p1{};
    if (axis == 0) {
      p0 = Vec3(axis_offset, r * c0, r * s0);
      p1 = Vec3(axis_offset, r * c1, r * s1);
    } else if (axis == 1) {
      p0 = Vec3(r * c0, axis_offset, r * s0);
      p1 = Vec3(r * c1, axis_offset, r * s1);
    } else {
      p0 = Vec3(r * c0, r * s0, axis_offset);
      p1 = Vec3(r * c1, r * s1, axis_offset);
    }
    fn(p0, p1);
  }
}

template <typename Fn>
void emitCapsuleWire(float radius, float half_height, Fn&& fn) {
  const float hh = std::max(half_height, 0.0f);
  const float r = std::max(radius, 1e-4f);
  emitAxisRing(r, 2, hh, fn);
  emitAxisRing(r, 2, -hh, fn);
  fn(Vec3(r, 0.0f, -hh), Vec3(r, 0.0f, hh));
  fn(Vec3(-r, 0.0f, -hh), Vec3(-r, 0.0f, hh));
  fn(Vec3(0.0f, r, -hh), Vec3(0.0f, r, hh));
  fn(Vec3(0.0f, -r, -hh), Vec3(0.0f, -r, hh));
}

}  // namespace collision_gizmo_detail

template <typename Fn>
void forEachColliderWireSegment(const ColliderComponent& collider, Fn&& fn) {
  switch (collider.shape) {
    case ColliderShapeKind::Box: {
      const Vec3 h = collider.box_half_extents;
      const Vec3 c0(-h.x, -h.y, -h.z);
      const Vec3 c1(h.x, -h.y, -h.z);
      const Vec3 c2(h.x, h.y, -h.z);
      const Vec3 c3(-h.x, h.y, -h.z);
      const Vec3 c4(-h.x, -h.y, h.z);
      const Vec3 c5(h.x, -h.y, h.z);
      const Vec3 c6(h.x, h.y, h.z);
      const Vec3 c7(-h.x, h.y, h.z);
      fn(c0, c1);
      fn(c1, c2);
      fn(c2, c3);
      fn(c3, c0);
      fn(c4, c5);
      fn(c5, c6);
      fn(c6, c7);
      fn(c7, c4);
      fn(c0, c4);
      fn(c1, c5);
      fn(c2, c6);
      fn(c3, c7);
      break;
    }
    case ColliderShapeKind::Sphere: {
      collision_gizmo_detail::emitAxisRing(collider.sphere_radius, 0, 0.0f, fn);
      collision_gizmo_detail::emitAxisRing(collider.sphere_radius, 1, 0.0f, fn);
      collision_gizmo_detail::emitAxisRing(collider.sphere_radius, 2, 0.0f, fn);
      break;
    }
    case ColliderShapeKind::Capsule: {
      collision_gizmo_detail::emitCapsuleWire(
          collider.capsule_radius, colliderCapsuleHalfHeight(collider), fn);
      break;
    }
    case ColliderShapeKind::TriangleMesh: {
      for (const ColliderTriangle& tri : collider.triangles) {
        fn(tri.v0, tri.v1);
        fn(tri.v1, tri.v2);
        fn(tri.v2, tri.v0);
      }
      break;
    }
  }
}

template <typename Fn>
void forEachCharacterControllerWireSegment(const CharacterControllerComponent& cct,
                                           Fn&& fn) {
  collision_gizmo_detail::emitCapsuleWire(
      cct.radius, characterControllerHalfHeight(cct), fn);
}

}  // namespace Blunder
