#pragma once

#include "EASTL/vector.h"

#include "runtime/core/math/math_types.h"

#include <algorithm>
#include <cstdint>

namespace Blunder {

enum class ColliderShapeKind : uint8_t {
  Box = 0,
  Sphere = 1,
  Capsule = 2,
  TriangleMesh = 3,
};

enum class ColliderBodyKind : uint8_t {
  Static = 0,
  Kinematic = 1,
  Area = 2,
};

struct ColliderTriangle {
  Vec3 v0{0.0f};
  Vec3 v1{0.0f};
  Vec3 v2{0.0f};
};

struct ColliderComponent final {
  ColliderShapeKind shape{ColliderShapeKind::Box};
  ColliderBodyKind body_kind{ColliderBodyKind::Static};
  uint32_t layer{1u};
  uint32_t mask{0xFFFFFFFFu};
  Vec3 box_half_extents{1.0f, 1.0f, 1.0f};
  float sphere_radius{0.5f};
  float capsule_radius{0.5f};
  float capsule_height{2.0f};
  eastl::vector<ColliderTriangle> triangles;
};

inline void sanitizeColliderComponent(ColliderComponent& collider) {
  collider.box_half_extents.x = std::max(collider.box_half_extents.x, 1e-4f);
  collider.box_half_extents.y = std::max(collider.box_half_extents.y, 1e-4f);
  collider.box_half_extents.z = std::max(collider.box_half_extents.z, 1e-4f);
  collider.sphere_radius = std::max(collider.sphere_radius, 1e-4f);
  collider.capsule_radius = std::max(collider.capsule_radius, 1e-4f);
  collider.capsule_height = std::max(collider.capsule_height, collider.capsule_radius * 2.0f);
  if (collider.layer == 0) {
    collider.layer = 1u;
  }
}

inline bool colliderComponentsEqual(const ColliderComponent& a, const ColliderComponent& b) {
  if (a.shape != b.shape || a.body_kind != b.body_kind || a.layer != b.layer || a.mask != b.mask ||
      a.box_half_extents != b.box_half_extents || a.sphere_radius != b.sphere_radius ||
      a.capsule_radius != b.capsule_radius || a.capsule_height != b.capsule_height ||
      a.triangles.size() != b.triangles.size()) {
    return false;
  }
  for (size_t i = 0; i < a.triangles.size(); ++i) {
    if (a.triangles[i].v0 != b.triangles[i].v0 || a.triangles[i].v1 != b.triangles[i].v1 ||
        a.triangles[i].v2 != b.triangles[i].v2) {
      return false;
    }
  }
  return true;
}

inline float colliderCapsuleHalfHeight(const ColliderComponent& collider) {
  return std::max(0.0f, collider.capsule_height * 0.5f - collider.capsule_radius);
}

inline const char* colliderShapeKindJson(ColliderShapeKind shape) {
  switch (shape) {
    case ColliderShapeKind::Sphere:
      return "sphere";
    case ColliderShapeKind::Capsule:
      return "capsule";
    case ColliderShapeKind::TriangleMesh:
      return "triangleMesh";
    case ColliderShapeKind::Box:
    default:
      return "box";
  }
}

inline bool colliderShapeKindFromJson(const eastl::string& text, ColliderShapeKind& out) {
  if (text == "sphere") {
    out = ColliderShapeKind::Sphere;
    return true;
  }
  if (text == "capsule") {
    out = ColliderShapeKind::Capsule;
    return true;
  }
  if (text == "triangleMesh" || text == "trimesh" || text == "triangle_mesh") {
    out = ColliderShapeKind::TriangleMesh;
    return true;
  }
  if (text == "box") {
    out = ColliderShapeKind::Box;
    return true;
  }
  return false;
}

inline const char* colliderBodyKindJson(ColliderBodyKind kind) {
  switch (kind) {
    case ColliderBodyKind::Kinematic:
      return "kinematic";
    case ColliderBodyKind::Area:
      return "area";
    case ColliderBodyKind::Static:
    default:
      return "static";
  }
}

inline bool colliderBodyKindFromJson(const eastl::string& text, ColliderBodyKind& out) {
  if (text == "kinematic") {
    out = ColliderBodyKind::Kinematic;
    return true;
  }
  if (text == "area") {
    out = ColliderBodyKind::Area;
    return true;
  }
  if (text == "static") {
    out = ColliderBodyKind::Static;
    return true;
  }
  return false;
}

}  // namespace Blunder
