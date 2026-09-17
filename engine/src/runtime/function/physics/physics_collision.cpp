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

FixedVec3 closestPointOnTriangle(FixedVec3 a, FixedVec3 b, FixedVec3 c, FixedVec3 p) {
  const FixedVec3 ab = b - a;
  const FixedVec3 ac = c - a;
  const FixedVec3 ap = p - a;
  const Fixed d1 = dot(ab, ap);
  const Fixed d2 = dot(ac, ap);
  if (d1.raw() <= 0 && d2.raw() <= 0) {
    return a;
  }

  const FixedVec3 bp = p - b;
  const Fixed d3 = dot(ab, bp);
  const Fixed d4 = dot(ac, bp);
  if (d3.raw() >= 0 && d4.raw() <= d3.raw()) {
    return b;
  }

  const Fixed vc = d1 * d4 - d3 * d2;
  if (vc.raw() <= 0 && d1.raw() >= 0 && d3.raw() <= 0) {
    const Fixed denom = d1 - d3;
    if (denom.raw() == 0) {
      return a;
    }
    const Fixed v = d1 / denom;
    return a + ab * v;
  }

  const FixedVec3 cp = p - c;
  const Fixed d5 = dot(ab, cp);
  const Fixed d6 = dot(ac, cp);
  if (d6.raw() >= 0 && d5.raw() <= d6.raw()) {
    return c;
  }

  const Fixed vb = d5 * d2 - d1 * d6;
  if (vb.raw() <= 0 && d2.raw() >= 0 && d6.raw() <= 0) {
    const Fixed denom = d2 - d6;
    if (denom.raw() == 0) {
      return a;
    }
    const Fixed w = d2 / denom;
    return a + ac * w;
  }

  const Fixed va = d3 * d6 - d5 * d4;
  if (va.raw() <= 0 && (d4 - d3).raw() >= 0 && (d5 - d6).raw() >= 0) {
    const Fixed denom = (d4 - d3) + (d5 - d6);
    if (denom.raw() == 0) {
      return b;
    }
    const Fixed w = (d4 - d3) / denom;
    return b + (c - b) * w;
  }

  const Fixed denom = va + vb + vc;
  if (denom.raw() == 0) {
    return a;
  }
  const Fixed v = vb / denom;
  const Fixed w = vc / denom;
  return a + ab * v + ac * w;
}

FixedVec3 triangleNormal(const PhysicsTriangle& tri) {
  const FixedVec3 n = cross(tri.v1 - tri.v0, tri.v2 - tri.v0);
  const Fixed len_sq = dot(n, n);
  if (len_sq.raw() == 0) {
    return FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1));
  }
  return n / sqrt(len_sq);
}

ContactManifold collideSphereTriangle(const ColliderWorldShape& sphere, const PhysicsTriangle& tri) {
  ContactManifold result{};
  const FixedVec3 center = transformPoint(sphere.pose, FixedVec3{});
  const FixedVec3 closest = closestPointOnTriangle(tri.v0, tri.v1, tri.v2, center);
  const FixedVec3 delta = center - closest;
  const Fixed dist_sq = dot(delta, delta);
  const Fixed radius = sphere.sphere_radius;
  const Fixed radius_sq = radius * radius;
  if (dist_sq.raw() >= radius_sq.raw()) {
    return result;
  }
  Fixed dist = sqrt(dist_sq);
  FixedVec3 normal = triangleNormal(tri);
  if (dist.raw() > 0) {
    normal = delta / dist;
  }
  result.valid = true;
  result.normal = normal;
  result.penetration = radius - dist;
  result.point_on_a = center - normal * radius;
  return result;
}

ContactManifold collideCapsuleTriangle(const ColliderWorldShape& capsule, const PhysicsTriangle& tri) {
  ContactManifold result{};
  FixedVec3 seg_a{};
  FixedVec3 seg_b{};
  getCapsuleSegment(capsule, seg_a, seg_b);
  const FixedVec3 mid = (seg_a + seg_b) * Fixed::from_raw(Fixed::kOne / 2);
  const FixedVec3 closest_tri = closestPointOnTriangle(tri.v0, tri.v1, tri.v2, mid);
  const FixedVec3 closest_seg = closestPointOnSegment(seg_a, seg_b, closest_tri);
  const FixedVec3 closest = closestPointOnTriangle(tri.v0, tri.v1, tri.v2, closest_seg);
  const FixedVec3 delta = closest_seg - closest;
  const Fixed dist_sq = dot(delta, delta);
  const Fixed radius = capsule.capsule_radius;
  const Fixed radius_sq = radius * radius;
  if (dist_sq.raw() >= radius_sq.raw()) {
    return result;
  }
  Fixed dist = sqrt(dist_sq);
  FixedVec3 normal = triangleNormal(tri);
  if (dist.raw() > 0) {
    normal = delta / dist;
  }
  result.valid = true;
  result.normal = normal;
  result.penetration = radius - dist;
  result.point_on_a = closest_seg - normal * radius;
  return result;
}

ContactManifold collideBoxTriangle(const ColliderWorldShape& box, const PhysicsTriangle& tri) {
  ColliderWorldShape sphere{};
  sphere.shape = ColliderShape::Sphere;
  sphere.pose = box.pose;
  Fixed max_extent = box.box_half_extents.x;
  if (box.box_half_extents.y.raw() > max_extent.raw()) {
    max_extent = box.box_half_extents.y;
  }
  if (box.box_half_extents.z.raw() > max_extent.raw()) {
    max_extent = box.box_half_extents.z;
  }
  sphere.sphere_radius = max_extent;
  return collideSphereTriangle(sphere, tri);
}

PhysicsTriangle triangleInWorld(const PhysicsTransform& pose, const PhysicsTriangle& local) {
  PhysicsTriangle world{};
  world.v0 = transformPoint(pose, local.v0);
  world.v1 = transformPoint(pose, local.v1);
  world.v2 = transformPoint(pose, local.v2);
  return world;
}

ContactManifold collideAgainstMesh(const ColliderWorldShape& other, const ColliderWorldShape& mesh,
                                   bool other_is_a) {
  ContactManifold best{};
  if (mesh.triangles == nullptr || mesh.triangle_count == 0) {
    return best;
  }
  for (uint32_t i = 0; i < mesh.triangle_count; ++i) {
    const PhysicsTriangle tri = triangleInWorld(mesh.pose, mesh.triangles[i]);
    ContactManifold hit{};
    if (other.shape == ColliderShape::Sphere) {
      hit = collideSphereTriangle(other, tri);
    } else if (other.shape == ColliderShape::Capsule) {
      hit = collideCapsuleTriangle(other, tri);
    } else if (other.shape == ColliderShape::Box) {
      hit = collideBoxTriangle(other, tri);
    }
    if (!hit.valid) {
      continue;
    }
    if (!best.valid || hit.penetration.raw() > best.penetration.raw()) {
      best = hit;
    }
  }
  if (best.valid && !other_is_a) {
    best.normal = best.normal * Fixed::from_int(-1);
  }
  return best;
}

bool rayTriangle(FixedVec3 origin, FixedVec3 dir, Fixed max_distance, const PhysicsTriangle& tri,
                 Fixed& out_t, FixedVec3& out_normal) {
  const FixedVec3 e1 = tri.v1 - tri.v0;
  const FixedVec3 e2 = tri.v2 - tri.v0;
  const FixedVec3 pvec = cross(dir, e2);
  const Fixed det = dot(e1, pvec);
  if (det.raw() == 0) {
    return false;
  }
  const Fixed inv_det = Fixed::from_int(1) / det;
  const FixedVec3 tvec = origin - tri.v0;
  const Fixed u = dot(tvec, pvec) * inv_det;
  if (u.raw() < 0 || u.raw() > Fixed::from_int(1).raw()) {
    return false;
  }
  const FixedVec3 qvec = cross(tvec, e1);
  const Fixed v = dot(dir, qvec) * inv_det;
  if (v.raw() < 0 || (u + v).raw() > Fixed::from_int(1).raw()) {
    return false;
  }
  const Fixed t = dot(e2, qvec) * inv_det;
  if (t.raw() < 0 || t.raw() > max_distance.raw()) {
    return false;
  }
  out_t = t;
  out_normal = triangleNormal(tri);
  if (dot(out_normal, dir).raw() > 0) {
    out_normal = out_normal * Fixed::from_int(-1);
  }
  return true;
}

bool raySphere(FixedVec3 origin, FixedVec3 dir, Fixed max_distance, FixedVec3 center, Fixed radius,
               Fixed& out_t, FixedVec3& out_normal) {
  const FixedVec3 oc = origin - center;
  const Fixed a = dot(dir, dir);
  if (a.raw() == 0) {
    return false;
  }
  const Fixed b = Fixed::from_int(2) * dot(oc, dir);
  const Fixed c = dot(oc, oc) - radius * radius;
  const Fixed disc = b * b - Fixed::from_int(4) * a * c;
  if (disc.raw() < 0) {
    return false;
  }
  const Fixed sqrt_disc = sqrt(disc);
  const Fixed two_a = Fixed::from_int(2) * a;
  Fixed t = (-b - sqrt_disc) / two_a;
  if (t.raw() < 0) {
    t = (-b + sqrt_disc) / two_a;
  }
  if (t.raw() < 0 || t.raw() > max_distance.raw()) {
    return false;
  }
  out_t = t;
  const FixedVec3 point = origin + dir * t;
  const FixedVec3 n = point - center;
  const Fixed n_len_sq = dot(n, n);
  out_normal = n_len_sq.raw() == 0 ? FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1))
                                   : n / sqrt(n_len_sq);
  return true;
}

bool rayBox(FixedVec3 origin, FixedVec3 dir, Fixed max_distance, const ColliderWorldShape& box,
            Fixed& out_t, FixedVec3& out_normal) {
  const FixedVec3 center = transformPoint(box.pose, FixedVec3{});
  const FixedQuat inv(Fixed::from_int(0) - box.pose.rotation.x, Fixed::from_int(0) - box.pose.rotation.y,
                      Fixed::from_int(0) - box.pose.rotation.z, box.pose.rotation.w);
  const FixedVec3 local_origin = rotateVec(inv, origin - center);
  const FixedVec3 local_dir = rotateVec(inv, dir);
  const FixedVec3 he = box.box_half_extents;

  Fixed tmin = Fixed::zero();
  Fixed tmax = max_distance;
  int hit_axis = 2;
  int hit_sign = 1;

  const Fixed comps[3] = {local_origin.x, local_origin.y, local_origin.z};
  const Fixed dirs[3] = {local_dir.x, local_dir.y, local_dir.z};
  const Fixed extents[3] = {he.x, he.y, he.z};
  for (int i = 0; i < 3; ++i) {
    if (dirs[i].raw() == 0) {
      if (absFixed(comps[i]).raw() > extents[i].raw()) {
        return false;
      }
      continue;
    }
    const Fixed inv_d = Fixed::from_int(1) / dirs[i];
    Fixed t1 = (-extents[i] - comps[i]) * inv_d;
    Fixed t2 = (extents[i] - comps[i]) * inv_d;
    int sign = -1;
    if (t1.raw() > t2.raw()) {
      const Fixed tmp = t1;
      t1 = t2;
      t2 = tmp;
      sign = 1;
    }
    if (t1.raw() > tmin.raw()) {
      tmin = t1;
      hit_axis = i;
      hit_sign = sign;
    }
    if (t2.raw() < tmax.raw()) {
      tmax = t2;
    }
    if (tmin.raw() > tmax.raw()) {
      return false;
    }
  }
  if (tmin.raw() < 0) {
    if (tmax.raw() < 0 || tmax.raw() > max_distance.raw()) {
      return false;
    }
    out_t = tmax;
  } else {
    if (tmin.raw() > max_distance.raw()) {
      return false;
    }
    out_t = tmin;
  }
  FixedVec3 local_n{};
  if (hit_axis == 0) {
    local_n.x = Fixed::from_int(hit_sign);
  } else if (hit_axis == 1) {
    local_n.y = Fixed::from_int(hit_sign);
  } else {
    local_n.z = Fixed::from_int(hit_sign);
  }
  out_normal = transformDirection(box.pose, local_n);
  return true;
}

ContactManifold dispatchPair(const ColliderWorldShape& a, const ColliderWorldShape& b) {
  const ColliderShape shape_a = a.shape;
  const ColliderShape shape_b = b.shape;

  if (shape_a == ColliderShape::TriangleMesh && shape_b == ColliderShape::TriangleMesh) {
    return ContactManifold{};
  }
  if (shape_a == ColliderShape::TriangleMesh) {
    return collideAgainstMesh(b, a, false);
  }
  if (shape_b == ColliderShape::TriangleMesh) {
    return collideAgainstMesh(a, b, true);
  }

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

bool raycastShape(const ColliderWorldShape& shape, FixedVec3 origin, FixedVec3 direction,
                  Fixed max_distance, Fixed& out_t, FixedVec3& out_point, FixedVec3& out_normal) {
  const Fixed dir_len_sq = dot(direction, direction);
  if (dir_len_sq.raw() == 0 || max_distance.raw() <= 0) {
    return false;
  }
  const FixedVec3 dir = direction / sqrt(dir_len_sq);
  bool hit = false;
  Fixed best_t = max_distance;
  FixedVec3 best_n{};

  if (shape.shape == ColliderShape::Sphere) {
    hit = raySphere(origin, dir, max_distance, transformPoint(shape.pose, FixedVec3{}),
                    shape.sphere_radius, best_t, best_n);
  } else if (shape.shape == ColliderShape::Box) {
    hit = rayBox(origin, dir, max_distance, shape, best_t, best_n);
  } else if (shape.shape == ColliderShape::Capsule) {
    FixedVec3 seg_a{};
    FixedVec3 seg_b{};
    getCapsuleSegment(shape, seg_a, seg_b);
    Fixed t = Fixed::zero();
    FixedVec3 n{};
    if (raySphere(origin, dir, max_distance, seg_a, shape.capsule_radius, t, n) &&
        (!hit || t.raw() < best_t.raw())) {
      hit = true;
      best_t = t;
      best_n = n;
    }
    if (raySphere(origin, dir, max_distance, seg_b, shape.capsule_radius, t, n) &&
        (!hit || t.raw() < best_t.raw())) {
      hit = true;
      best_t = t;
      best_n = n;
    }
  } else if (shape.shape == ColliderShape::TriangleMesh && shape.triangles != nullptr) {
    for (uint32_t i = 0; i < shape.triangle_count; ++i) {
      Fixed t = Fixed::zero();
      FixedVec3 n{};
      const PhysicsTriangle tri = triangleInWorld(shape.pose, shape.triangles[i]);
      if (rayTriangle(origin, dir, max_distance, tri, t, n) &&
          (!hit || t.raw() < best_t.raw())) {
        hit = true;
        best_t = t;
        best_n = n;
      }
    }
  }

  if (!hit) {
    return false;
  }
  out_t = best_t;
  out_point = origin + dir * best_t;
  out_normal = best_n;
  return true;
}

}  // namespace Blunder
