#include "function/physics/physics_collision.h"

#include <cassert>
#include <cstdint>

namespace {

using Blunder::ColliderShape;
using Blunder::ColliderWorldShape;
using Blunder::ContactManifold;
using Blunder::Fixed;
using Blunder::FixedQuat;
using Blunder::FixedVec3;
using Blunder::PhysicsTransform;

ColliderWorldShape makeBox(FixedVec3 position, FixedVec3 half_extents) {
  ColliderWorldShape shape{};
  shape.shape = ColliderShape::Box;
  shape.pose.position = position;
  shape.box_half_extents = half_extents;
  return shape;
}

ColliderWorldShape makeSphere(FixedVec3 position, Fixed radius) {
  ColliderWorldShape shape{};
  shape.shape = ColliderShape::Sphere;
  shape.pose.position = position;
  shape.sphere_radius = radius;
  return shape;
}

ColliderWorldShape makeCapsule(FixedVec3 position, Fixed radius, Fixed half_height) {
  ColliderWorldShape shape{};
  shape.shape = ColliderShape::Capsule;
  shape.pose.position = position;
  shape.capsule_radius = radius;
  shape.capsule_half_height = half_height;
  return shape;
}

void overlapping_boxes_generate_contact() {
  const ColliderWorldShape box_a = makeBox(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()),
                                           FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  const ColliderWorldShape box_b = makeBox(FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero()),
                                           FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  const ContactManifold contact = Blunder::collide(box_a, box_b);
  assert(contact.valid);
  assert(contact.penetration.raw() > 0);
}

void separated_boxes_no_contact() {
  const ColliderWorldShape box_a = makeBox(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()),
                                           FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  const ColliderWorldShape box_b = makeBox(FixedVec3(Fixed::from_int(5), Fixed::zero(), Fixed::zero()),
                                           FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  const ContactManifold contact = Blunder::collide(box_a, box_b);
  assert(!contact.valid);
}

void overlapping_spheres_generate_contact() {
  const ColliderWorldShape sphere_a = makeSphere(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()), Fixed::from_int(1));
  const ColliderWorldShape sphere_b =
      makeSphere(FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero()), Fixed::from_int(1));
  const ContactManifold contact = Blunder::collide(sphere_a, sphere_b);
  assert(contact.valid);
  assert(contact.penetration.raw() > 0);
}

void sphere_on_box_generates_contact() {
  const ColliderWorldShape sphere =
      makeSphere(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(1)), Fixed::from_int(1));
  const ColliderWorldShape box = makeBox(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()),
                                         FixedVec3(Fixed::from_int(2), Fixed::from_int(2), Fixed::from_int(1)));
  const ContactManifold contact = Blunder::collide(sphere, box);
  assert(contact.valid);
}

void stable_pair_order_independent() {
  const ColliderWorldShape box_a = makeBox(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()),
                                           FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  const ColliderWorldShape box_b = makeBox(FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero()),
                                           FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));
  const ContactManifold ab = Blunder::collide(box_a, box_b);
  const ContactManifold ba = Blunder::collide(box_b, box_a);
  assert(ab.valid && ba.valid);
  assert(ab.penetration == ba.penetration);
  assert(ab.normal.x == -ba.normal.x);
}

void ray_hits_capsule_shaft() {
  const ColliderWorldShape cap =
      makeCapsule(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()),
                  Fixed::from_int(1) / Fixed::from_int(2), Fixed::from_int(1));
  Fixed t = Fixed::zero();
  FixedVec3 point{};
  FixedVec3 normal{};
  const bool hit =
      Blunder::raycastShape(cap, FixedVec3(Fixed::from_int(5), Fixed::zero(), Fixed::zero()),
                            FixedVec3(Fixed::from_int(-1), Fixed::zero(), Fixed::zero()),
                            Fixed::from_int(20), t, point, normal);
  assert(hit);
  const Fixed expected = Fixed::from_int(5) - Fixed::from_int(1) / Fixed::from_int(2);
  const Fixed slop = Fixed::from_int(1) / Fixed::from_int(10);
  assert(t.raw() > (expected - slop).raw());
  assert(t.raw() < (expected + slop).raw());
}

void ray_misses_beside_capsule() {
  const ColliderWorldShape cap =
      makeCapsule(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()),
                  Fixed::from_int(1) / Fixed::from_int(2), Fixed::from_int(1));
  Fixed t = Fixed::zero();
  FixedVec3 point{};
  FixedVec3 normal{};
  const bool hit =
      Blunder::raycastShape(cap, FixedVec3(Fixed::from_int(5), Fixed::from_int(2), Fixed::zero()),
                            FixedVec3(Fixed::from_int(-1), Fixed::zero(), Fixed::zero()),
                            Fixed::from_int(20), t, point, normal);
  assert(!hit);
}

void ray_hits_capsule_end_sphere() {
  const ColliderWorldShape cap =
      makeCapsule(FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::zero()),
                  Fixed::from_int(1) / Fixed::from_int(2), Fixed::from_int(1));
  Fixed t = Fixed::zero();
  FixedVec3 point{};
  FixedVec3 normal{};
  const bool hit =
      Blunder::raycastShape(cap, FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(5)),
                            FixedVec3(Fixed::zero(), Fixed::zero(), Fixed::from_int(-1)),
                            Fixed::from_int(20), t, point, normal);
  assert(hit);
  const Fixed expected = Fixed::from_int(5) - (Fixed::from_int(1) + Fixed::from_int(1) / Fixed::from_int(2));
  const Fixed slop = Fixed::from_int(1) / Fixed::from_int(10);
  assert(t.raw() > (expected - slop).raw());
  assert(t.raw() < (expected + slop).raw());
}

}  // namespace

int main() {
  overlapping_boxes_generate_contact();
  separated_boxes_no_contact();
  overlapping_spheres_generate_contact();
  sphere_on_box_generates_contact();
  stable_pair_order_independent();
  ray_hits_capsule_shaft();
  ray_misses_beside_capsule();
  ray_hits_capsule_end_sphere();
  return 0;
}
