#include "function/physics/physics_collision.h"

#include <vector>

namespace Blunder {
namespace {

constexpr int kMaxBoxAxes = 15;

FixedVec3 cross(FixedVec3 a, FixedVec3 b) {
  return FixedVec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

FixedVec3 rotateVec(FixedQuat q, FixedVec3 v) {
  const FixedVec3 u(q.x, q.y, q.z);
  const Fixed s = q.w;
  const Fixed two = Fixed::from_int(2);
  return u * (two * dot(u, v)) + v * (s * s - dot(u, u)) + cross(u, v) * (two * s);
}

FixedVec3 transformPoint(const PhysicsTransform& pose, FixedVec3 local) {
  return pose.position + rotateVec(pose.rotation, local);
}

FixedVec3 transformDirection(const PhysicsTransform& pose, FixedVec3 local) {
  return rotateVec(pose.rotation, local);
}

Fixed absFixed(Fixed value) {
  return value.raw() < 0 ? -value : value;
}

Fixed minFixed(Fixed a, Fixed b) { return a.raw() < b.raw() ? a : b; }
Fixed maxFixed(Fixed a, Fixed b) { return a.raw() > b.raw() ? a : b; }

struct Interval {
  Fixed min = Fixed::zero();
  Fixed max = Fixed::zero();
};

Interval projectBoxOntoAxis(const ColliderWorldShape& box, FixedVec3 axis) {
  const FixedVec3 axes[3] = {transformDirection(box.pose, FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero())),
                             transformDirection(box.pose, FixedVec3(Fixed::zero(), Fixed::from_int(1), Fixed::zero())),
                             transformDirection(box.pose, FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1)))};
  const Fixed extents[3] = {box.box_half_extents.x, box.box_half_extents.y, box.box_half_extents.z};

  Fixed min_proj = dot(transformPoint(box.pose, FixedVec3{}), axis);
  Fixed max_proj = min_proj;
  for (int i = 0; i < 3; ++i) {
    const Fixed extent = absFixed(dot(axes[i], axis)) * extents[i];
    min_proj = min_proj - extent;
    max_proj = max_proj + extent;
  }
  return Interval{min_proj, max_proj};
}

bool intervalsOverlap(Interval a, Interval b, Fixed& overlap) {
  const Fixed min_overlap = minFixed(a.max, b.max) - maxFixed(a.min, b.min);
  overlap = min_overlap;
  return min_overlap.raw() > 0;
}

ContactManifold collideBoxBox(const ColliderWorldShape& a, const ColliderWorldShape& b) {
  ContactManifold result{};
  Fixed best_overlap = Fixed::from_int(1000000);
  FixedVec3 best_axis{};

  const FixedVec3 axes_a[3] = {transformDirection(a.pose, FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero())),
                               transformDirection(a.pose, FixedVec3(Fixed::zero(), Fixed::from_int(1), Fixed::zero())),
                               transformDirection(a.pose, FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1)))};
  const FixedVec3 axes_b[3] = {transformDirection(b.pose, FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero())),
                               transformDirection(b.pose, FixedVec3(Fixed::zero(), Fixed::from_int(1), Fixed::zero())),
                               transformDirection(b.pose, FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1)))};

  FixedVec3 test_axes[kMaxBoxAxes];
  int axis_count = 0;
  for (int i = 0; i < 3; ++i) {
    test_axes[axis_count++] = axes_a[i];
    test_axes[axis_count++] = axes_b[i];
  }
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      const FixedVec3 cross_axis = cross(axes_a[i], axes_b[j]);
      if (cross_axis.x.raw() != 0 || cross_axis.y.raw() != 0 || cross_axis.z.raw() != 0) {
        test_axes[axis_count++] = normalize(cross_axis);
      }
    }
  }

  const FixedVec3 center_delta = transformPoint(b.pose, FixedVec3{}) - transformPoint(a.pose, FixedVec3{});

  for (int i = 0; i < axis_count; ++i) {
    FixedVec3 axis = test_axes[i];
    if (axis.x.raw() == 0 && axis.y.raw() == 0 && axis.z.raw() == 0) {
      continue;
    }

    const Interval proj_a = projectBoxOntoAxis(a, axis);
    const Interval proj_b = projectBoxOntoAxis(b, axis);
    Fixed overlap = Fixed::zero();
    if (!intervalsOverlap(proj_a, proj_b, overlap)) {
      return result;
    }

    if (dot(center_delta, axis).raw() < 0) {
      axis = axis * Fixed::from_int(-1);
    }

    if (overlap.raw() < best_overlap.raw()) {
      best_overlap = overlap;
      best_axis = axis;
    }
  }

  if (best_overlap.raw() >= Fixed::from_int(1000000).raw()) {
    return result;
  }

  if (center_delta.x.raw() == 0 && center_delta.y.raw() == 0 && center_delta.z.raw() == 0) {
    if (best_axis.z.raw() < 0) {
      best_axis = best_axis * Fixed::from_int(-1);
    }
    if (best_axis.z.raw() == 0) {
      best_axis = FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1));
    }
  }

  result.valid = true;
  result.penetration = best_overlap;
  result.normal = best_axis;
  result.point_on_a = transformPoint(a.pose, FixedVec3{}) + best_axis * a.box_half_extents.z;
  return result;
}

ContactManifold collideSphereSphere(const ColliderWorldShape& a, const ColliderWorldShape& b) {
  ContactManifold result{};
  const FixedVec3 center_a = transformPoint(a.pose, FixedVec3{});
  const FixedVec3 center_b = transformPoint(b.pose, FixedVec3{});
  const FixedVec3 delta = center_b - center_a;
  const Fixed dist_sq = dot(delta, delta);
  const Fixed radius_sum = a.sphere_radius + b.sphere_radius;
  const Fixed radius_sum_sq = radius_sum * radius_sum;

  if (dist_sq.raw() >= radius_sum_sq.raw()) {
    return result;
  }

  Fixed dist = sqrt(dist_sq);
  FixedVec3 normal = FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero());
  if (dist.raw() > 0) {
    normal = delta / dist;
  } else {
    normal = FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1));
    dist = Fixed::zero();
  }

  result.valid = true;
  result.normal = normal;
  result.penetration = radius_sum - dist;
  result.point_on_a = center_a + normal * a.sphere_radius;
  return result;
}

FixedVec3 closestPointOnSegment(FixedVec3 a, FixedVec3 b, FixedVec3 point) {
  const FixedVec3 ab = b - a;
  const Fixed ab_len_sq = dot(ab, ab);
  if (ab_len_sq.raw() == 0) {
    return a;
  }
  Fixed t = dot(point - a, ab) / ab_len_sq;
  if (t.raw() < 0) {
    t = Fixed::zero();
  }
  if (t.raw() > Fixed::from_int(1).raw()) {
    t = Fixed::from_int(1);
  }
  return a + ab * t;
}

void getCapsuleSegment(const ColliderWorldShape& capsule, FixedVec3& out_a, FixedVec3& out_b) {
  const FixedVec3 local_a( Fixed::zero(), Fixed::zero(), -capsule.capsule_half_height);
  const FixedVec3 local_b( Fixed::zero(), Fixed::zero(), capsule.capsule_half_height);
  out_a = transformPoint(capsule.pose, local_a);
  out_b = transformPoint(capsule.pose, local_b);
}

ContactManifold collideSphereBox(const ColliderWorldShape& sphere, const ColliderWorldShape& box) {
  ContactManifold result{};
  const FixedVec3 sphere_center = transformPoint(sphere.pose, FixedVec3{});
  const FixedVec3 box_center = transformPoint(box.pose, FixedVec3{});
  const FixedVec3 local = rotateVec(
      FixedQuat(-box.pose.rotation.x, -box.pose.rotation.y, -box.pose.rotation.z, box.pose.rotation.w),
      sphere_center - box_center);

  const FixedVec3 clamped(minFixed(maxFixed(local.x, -box.box_half_extents.x), box.box_half_extents.x),
                          minFixed(maxFixed(local.y, -box.box_half_extents.y), box.box_half_extents.y),
                          minFixed(maxFixed(local.z, -box.box_half_extents.z), box.box_half_extents.z));

  const FixedVec3 delta_local = local - clamped;
  const Fixed dist_sq = dot(delta_local, delta_local);
  const Fixed radius = sphere.sphere_radius;

  const Fixed radius_sq = radius * radius;

  if (dist_sq.raw() > radius_sq.raw()) {
    return result;
  }

  Fixed dist = sqrt(dist_sq);
  FixedVec3 normal_local = FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1));
  if (dist.raw() > 0) {
    normal_local = delta_local / dist;
  }

  const FixedVec3 normal = transformDirection(box.pose, normal_local);
  result.valid = true;
  result.normal = normal;
  result.penetration = radius - dist;
  result.point_on_a = sphere_center - normal * sphere.sphere_radius;
  return result;
}

ContactManifold collideCapsuleCapsule(const ColliderWorldShape& a, const ColliderWorldShape& b) {
  ContactManifold result{};
  FixedVec3 seg_a0{};
  FixedVec3 seg_a1{};
  FixedVec3 seg_b0{};
  FixedVec3 seg_b1{};
  getCapsuleSegment(a, seg_a0, seg_a1);
  getCapsuleSegment(b, seg_b0, seg_b1);

  const FixedVec3 mid_a = (seg_a0 + seg_a1) * Fixed::from_raw(Fixed::kOne / 2);
  const FixedVec3 mid_b = (seg_b0 + seg_b1) * Fixed::from_raw(Fixed::kOne / 2);
  const FixedVec3 closest_on_b = closestPointOnSegment(seg_b0, seg_b1, mid_a);
  const FixedVec3 closest_on_a = closestPointOnSegment(seg_a0, seg_a1, closest_on_b);

  const FixedVec3 delta = closest_on_b - closest_on_a;
  const Fixed dist_sq = dot(delta, delta);
  const Fixed radius_sum = a.capsule_radius + b.capsule_radius;

  const Fixed radius_sum_sq = radius_sum * radius_sum;

  if (dist_sq.raw() >= radius_sum_sq.raw()) {
    return result;
  }

  Fixed dist = sqrt(dist_sq);
  FixedVec3 normal = FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero());
  if (dist.raw() > 0) {
    normal = delta / dist;
  } else {
    normal = FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1));
    dist = Fixed::zero();
  }

  result.valid = true;
  result.normal = normal;
  result.penetration = radius_sum - dist;
  result.point_on_a = closest_on_a + normal * a.capsule_radius;
  return result;
}

ContactManifold collideCapsuleSphere(const ColliderWorldShape& capsule, const ColliderWorldShape& sphere) {
  ContactManifold result{};
  FixedVec3 seg_a{};
  FixedVec3 seg_b{};
  getCapsuleSegment(capsule, seg_a, seg_b);
  const FixedVec3 sphere_center = transformPoint(sphere.pose, FixedVec3{});
  const FixedVec3 closest = closestPointOnSegment(seg_a, seg_b, sphere_center);
  const FixedVec3 delta = sphere_center - closest;
  const Fixed dist_sq = dot(delta, delta);
  const Fixed radius_sum = capsule.capsule_radius + sphere.sphere_radius;
  const Fixed radius_sum_sq_capsule = radius_sum * radius_sum;

  if (dist_sq.raw() >= radius_sum_sq_capsule.raw()) {
    return result;
  }

  Fixed dist = sqrt(dist_sq);
  FixedVec3 normal = FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero());
  if (dist.raw() > 0) {
    normal = delta / dist;
  } else {
    normal = FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1));
    dist = Fixed::zero();
  }

  result.valid = true;
  result.normal = normal;
  result.penetration = radius_sum - dist;
  result.point_on_a = closest + normal * capsule.capsule_radius;
  return result;
}

ContactManifold collideCapsuleBox(const ColliderWorldShape& capsule, const ColliderWorldShape& box) {
  ColliderWorldShape proxy_sphere{};
  proxy_sphere.shape = ColliderShape::Sphere;
  FixedVec3 seg_a{};
  FixedVec3 seg_b{};
  getCapsuleSegment(capsule, seg_a, seg_b);
  proxy_sphere.pose.position = (seg_a + seg_b) * Fixed::from_raw(Fixed::kOne / 2);
  proxy_sphere.pose.rotation = FixedQuat{};
  proxy_sphere.sphere_radius = capsule.capsule_radius + capsule.capsule_half_height;
  return collideSphereBox(proxy_sphere, box);
}

ContactManifold dispatchPair(const ColliderWorldShape& a, const ColliderWorldShape& b) {
  const ColliderShape shape_a = a.shape;
  const ColliderShape shape_b = b.shape;

  if (shape_a == ColliderShape::Box && shape_b == ColliderShape::Box) {
    return collideBoxBox(a, b);
  }
  if (shape_a == ColliderShape::Sphere && shape_b == ColliderShape::Sphere) {
    return collideSphereSphere(a, b);
  }
  if (shape_a == ColliderShape::Sphere && shape_b == ColliderShape::Box) {
    return collideSphereBox(a, b);
  }
  if (shape_a == ColliderShape::Box && shape_b == ColliderShape::Sphere) {
    ContactManifold flipped = collideSphereBox(b, a);
    if (flipped.valid) {
      flipped.normal = flipped.normal * Fixed::from_int(-1);
    }
    return flipped;
  }
  if (shape_a == ColliderShape::Capsule && shape_b == ColliderShape::Capsule) {
    return collideCapsuleCapsule(a, b);
  }
  if (shape_a == ColliderShape::Capsule && shape_b == ColliderShape::Sphere) {
    return collideCapsuleSphere(a, b);
  }
  if (shape_a == ColliderShape::Sphere && shape_b == ColliderShape::Capsule) {
    ContactManifold flipped = collideCapsuleSphere(b, a);
    if (flipped.valid) {
      flipped.normal = flipped.normal * Fixed::from_int(-1);
    }
    return flipped;
  }
  if (shape_a == ColliderShape::Capsule && shape_b == ColliderShape::Box) {
    return collideCapsuleBox(a, b);
  }
  if (shape_a == ColliderShape::Box && shape_b == ColliderShape::Capsule) {
    ContactManifold flipped = collideCapsuleBox(b, a);
    if (flipped.valid) {
      flipped.normal = flipped.normal * Fixed::from_int(-1);
    }
    return flipped;
  }

  return ContactManifold{};
}

}  // namespace

ContactManifold collide(const ColliderWorldShape& a, const ColliderWorldShape& b) {
  return dispatchPair(a, b);
}

}  // namespace Blunder
