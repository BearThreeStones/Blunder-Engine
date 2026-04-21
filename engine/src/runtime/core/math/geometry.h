#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <optional>

#include "EASTL/algorithm.h"
#include "EASTL/array.h"
#include "runtime/core/math/math_types.h"

namespace Blunder {

/// Result of a ray intersection test
struct RayHit {
  float t;      ///< Distance along ray
  Vec3 point;   ///< Hit point in world space
  Vec3 normal;  ///< Surface normal at hit point
};

/// Ray defined by origin and direction
struct Ray {
  Vec3 origin;
  Vec3 direction;  ///< Should be normalized

  /// Get point at parameter t along the ray
  Vec3 pointAt(float t) const { return origin + direction * t; }
};

/// Plane defined by normal and distance from origin
struct Plane {
  Vec3 normal;     ///< Unit normal vector
  float distance;  ///< Distance from origin (dot(normal, pointOnPlane))

  /// Create plane from normal and point on plane
  static Plane fromPointAndNormal(const Vec3& point, const Vec3& normal) {
    Vec3 n = glm::normalize(normal);
    return {n, glm::dot(n, point)};
  }

  /// Get signed distance from point to plane
  float signedDistanceTo(const Vec3& point) const {
    return glm::dot(normal, point) - distance;
  }
};

/// Axis-Aligned Bounding Box
struct AABB {
  Vec3 min;
  Vec3 max;

  /// Create AABB from center and half-extents
  static AABB fromCenterExtents(const Vec3& center, const Vec3& extents) {
    return {center - extents, center + extents};
  }

  Vec3 center() const { return (min + max) * 0.5f; }
  Vec3 extents() const { return (max - min) * 0.5f; }
  Vec3 size() const { return max - min; }

  bool contains(const Vec3& point) const {
    return point.x >= min.x && point.x <= max.x && point.y >= min.y &&
           point.y <= max.y && point.z >= min.z && point.z <= max.z;
  }

  /// Expand AABB to include a point
  void expandToInclude(const Vec3& point) {
    min = glm::min(min, point);
    max = glm::max(max, point);
  }
};

/// Oriented Bounding Box
struct OBB {
  Vec3 center;
  Vec3 axes[3];      ///< Local X, Y, Z axes (orthonormal, unit vectors)
  Vec3 halfExtents;  ///< Half-size along each local axis

  /// Create OBB from AABB (axis-aligned)
  static OBB fromAABB(const AABB& aabb) {
    OBB obb;
    obb.center = aabb.center();
    obb.axes[0] = Vec3(1.0f, 0.0f, 0.0f);
    obb.axes[1] = Vec3(0.0f, 1.0f, 0.0f);
    obb.axes[2] = Vec3(0.0f, 0.0f, 1.0f);
    obb.halfExtents = aabb.extents();
    return obb;
  }

  /// Get corner point (index 0-7)
  Vec3 getCorner(int index) const {
    Vec3 corner = center;
    corner += axes[0] * ((index & 1) ? halfExtents.x : -halfExtents.x);
    corner += axes[1] * ((index & 2) ? halfExtents.y : -halfExtents.y);
    corner += axes[2] * ((index & 4) ? halfExtents.z : -halfExtents.z);
    return corner;
  }
};

// ============================================================================
// Intersection Tests
// ============================================================================

/// Test AABB-AABB overlap
inline bool intersects(const AABB& a, const AABB& b) {
  return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
         (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
         (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

/// Test AABB-Plane intersection (true if box intersects or crosses plane)
inline bool intersects(const AABB& box, const Plane& plane) {
  Vec3 c = box.center();
  Vec3 e = box.extents();

  // Compute the projection radius of box onto the plane normal
  float r = e.x * std::abs(plane.normal.x) + e.y * std::abs(plane.normal.y) +
            e.z * std::abs(plane.normal.z);

  // Compute distance from center to plane
  float s = plane.signedDistanceTo(c);

  return std::abs(s) <= r;
}

/// Ray-Plane intersection (slab method)
inline std::optional<RayHit> intersect(const Ray& ray, const Plane& plane) {
  float denom = glm::dot(plane.normal, ray.direction);

  // Ray parallel to plane
  if (std::abs(denom) < 1e-6f) {
    return std::nullopt;
  }

  float t = (plane.distance - glm::dot(plane.normal, ray.origin)) / denom;

  // Behind ray origin
  if (t < 0.0f) {
    return std::nullopt;
  }

  RayHit hit;
  hit.t = t;
  hit.point = ray.pointAt(t);
  hit.normal = denom < 0.0f ? plane.normal : -plane.normal;
  return hit;
}

/// Ray-AABB intersection (slab method)
inline std::optional<RayHit> intersect(const Ray& ray, const AABB& box) {
  Vec3 invDir = 1.0f / ray.direction;

  Vec3 t0 = (box.min - ray.origin) * invDir;
  Vec3 t1 = (box.max - ray.origin) * invDir;

  Vec3 tmin = glm::min(t0, t1);
  Vec3 tmax = glm::max(t0, t1);

  float tNear = std::max({tmin.x, tmin.y, tmin.z});
  float tFar = std::min({tmax.x, tmax.y, tmax.z});

  if (tNear > tFar || tFar < 0.0f) {
    return std::nullopt;
  }

  float t = tNear >= 0.0f ? tNear : tFar;

  RayHit hit;
  hit.t = t;
  hit.point = ray.pointAt(t);

  // Compute normal based on which slab was hit
  Vec3 c = box.center();
  Vec3 p = hit.point - c;
  Vec3 d = box.extents();
  float bias = 1.0001f;

  hit.normal = Vec3(0.0f);
  if (std::abs(p.x / d.x) > std::abs(p.y / d.y) &&
      std::abs(p.x / d.x) > std::abs(p.z / d.z)) {
    hit.normal.x = p.x > 0.0f ? 1.0f : -1.0f;
  } else if (std::abs(p.y / d.y) > std::abs(p.z / d.z)) {
    hit.normal.y = p.y > 0.0f ? 1.0f : -1.0f;
  } else {
    hit.normal.z = p.z > 0.0f ? 1.0f : -1.0f;
  }

  return hit;
}

/// Ray-OBB intersection
inline std::optional<RayHit> intersect(const Ray& ray, const OBB& obb) {
  // Transform ray to OBB local space
  Vec3 delta = ray.origin - obb.center;
  Vec3 localOrigin(glm::dot(delta, obb.axes[0]), glm::dot(delta, obb.axes[1]),
                   glm::dot(delta, obb.axes[2]));
  Vec3 localDir(glm::dot(ray.direction, obb.axes[0]),
                glm::dot(ray.direction, obb.axes[1]),
                glm::dot(ray.direction, obb.axes[2]));

  // Create local AABB
  AABB localBox{-obb.halfExtents, obb.halfExtents};
  Ray localRay{localOrigin, localDir};

  auto result = intersect(localRay, localBox);
  if (!result) {
    return std::nullopt;
  }

  // Transform result back to world space
  RayHit hit;
  hit.t = result->t;
  hit.point = ray.pointAt(hit.t);
  hit.normal = result->normal.x * obb.axes[0] + result->normal.y * obb.axes[1] +
               result->normal.z * obb.axes[2];

  return hit;
}

/// Project OBB onto axis and return min/max values
inline void projectOBB(const OBB& obb, const Vec3& axis, float& outMin,
                       float& outMax) {
  float centerProj = glm::dot(obb.center, axis);
  float extent = std::abs(glm::dot(obb.axes[0] * obb.halfExtents.x, axis)) +
                 std::abs(glm::dot(obb.axes[1] * obb.halfExtents.y, axis)) +
                 std::abs(glm::dot(obb.axes[2] * obb.halfExtents.z, axis));
  outMin = centerProj - extent;
  outMax = centerProj + extent;
}

/// Test OBB-OBB intersection using Separating Axis Theorem (SAT)
inline bool intersects(const OBB& a, const OBB& b) {
  constexpr float EPSILON = 1e-6f;

  // 15 axes to test:
  // 3 face normals from A
  // 3 face normals from B
  // 9 cross products of edges

  std::array<Vec3, 15> axes;
  int axisCount = 0;

  // A's face normals
  axes[axisCount++] = a.axes[0];
  axes[axisCount++] = a.axes[1];
  axes[axisCount++] = a.axes[2];

  // B's face normals
  axes[axisCount++] = b.axes[0];
  axes[axisCount++] = b.axes[1];
  axes[axisCount++] = b.axes[2];

  // Cross products
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      Vec3 cross = glm::cross(a.axes[i], b.axes[j]);
      float len = glm::length(cross);
      if (len > EPSILON) {
        axes[axisCount++] = cross / len;
      }
    }
  }

  // Test each axis
  for (int i = 0; i < axisCount; ++i) {
    float aMin, aMax, bMin, bMax;
    projectOBB(a, axes[i], aMin, aMax);
    projectOBB(b, axes[i], bMin, bMax);

    // Check for separation
    if (aMax < bMin || bMax < aMin) {
      return false;  // Separating axis found
    }
  }

  return true;  // No separating axis, boxes intersect
}

// Verify geometry types are trivially copyable
static_assert(std::is_trivially_copyable_v<Ray>,
              "Ray must be trivially copyable");
static_assert(std::is_trivially_copyable_v<Plane>,
              "Plane must be trivially copyable");
static_assert(std::is_trivially_copyable_v<AABB>,
              "AABB must be trivially copyable");
static_assert(std::is_trivially_copyable_v<OBB>,
              "OBB must be trivially copyable");
static_assert(std::is_trivially_copyable_v<RayHit>,
              "RayHit must be trivially copyable");

}  // namespace Blunder
