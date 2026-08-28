#include "physics_golden_dump.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

using Blunder::ColliderHandle;
using Blunder::Fixed;
using Blunder::FixedQuat;
using Blunder::FixedVec3;
using Blunder::MotionType;
using Blunder::PhysicsMaterial;
using Blunder::PhysicsTransform;
using Blunder::PhysicsWorld;
using Blunder::RigidBodyHandle;
using Blunder::PhysicsGolden::BodyState;
using Blunder::PhysicsGolden::ScenarioSnapshot;
using Blunder::PhysicsGolden::captureBody;
using Blunder::PhysicsGolden::compareDumpFile;
using Blunder::PhysicsGolden::writeDumpFile;

Fixed testDt() { return Fixed::from_int(1) / Fixed::from_int(60); }

Fixed testGravityZ() { return -(Fixed::from_int(981) / Fixed::from_int(100)); }

void assertBodyState(const BodyState& actual, const BodyState& expected, const char* scenario, size_t index) {
  if (actual.pos_x != expected.pos_x || actual.pos_y != expected.pos_y || actual.pos_z != expected.pos_z ||
      actual.quat_x != expected.quat_x || actual.quat_y != expected.quat_y || actual.quat_z != expected.quat_z ||
      actual.quat_w != expected.quat_w || actual.vel_x != expected.vel_x || actual.vel_y != expected.vel_y ||
      actual.vel_z != expected.vel_z || actual.sleeping != expected.sleeping) {
    std::fprintf(stderr, "physics_golden_suite: body state mismatch scenario '%s' body %zu\n", scenario, index);
    std::abort();
  }
}

void assertScenarioMatches(const ScenarioSnapshot& snapshot, const BodyState* expected, size_t expected_count) {
  if (snapshot.bodies.size() != expected_count) {
    std::fprintf(stderr, "physics_golden_suite: body count mismatch for '%s'\n",
                 snapshot.name ? snapshot.name : "");
    std::abort();
  }
  for (size_t i = 0; i < expected_count; ++i) {
    assertBodyState(snapshot.bodies[i], expected[i], snapshot.name ? snapshot.name : "", i);
  }
}

RigidBodyHandle makeFloor(PhysicsWorld& world) {
  const RigidBodyHandle floor = world.createRigidBody(MotionType::Static, PhysicsTransform{}, Fixed::zero());
  (void)world.attachBoxCollider(floor, FixedVec3(Fixed::from_int(10), Fixed::from_int(10), Fixed::from_int(1)));
  return floor;
}

ScenarioSnapshot scenario_free_fall() {
  PhysicsWorld* world = PhysicsWorld::create();
  const RigidBodyHandle body =
      world->createRigidBody(MotionType::Dynamic, PhysicsTransform{}, Fixed::from_int(1));

  const Fixed dt = testDt();
  for (int i = 0; i < 5; ++i) {
    world->step(dt);
  }

  ScenarioSnapshot snapshot{};
  snapshot.name = "free_fall";
  snapshot.bodies.push_back(captureBody(*world, body));

  const BodyState& state = snapshot.bodies[0];
  assert(state.pos_z < 0);
  assert(state.vel_z < 0);
  assert(state.sleeping == 0);

  static const BodyState kExpected[] = {
      {0, 0, -175556790, 0, 0, 0, 4294967296, 0, 0, -3511135755, 0},
  };
  assertScenarioMatches(snapshot, kExpected, 1);

  world->destroy();
  return snapshot;
}

ScenarioSnapshot scenario_dynamic_on_static() {
  PhysicsWorld* world = PhysicsWorld::create();

  makeFloor(*world);

  PhysicsTransform box_pose{};
  box_pose.position.z = Fixed::from_int(5);
  const RigidBodyHandle box = world->createRigidBody(MotionType::Dynamic, box_pose, Fixed::from_int(1));
  (void)world->attachBoxCollider(box, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));

  const Fixed dt = testDt();
  for (int i = 0; i < 240; ++i) {
    world->step(dt);
  }

  ScenarioSnapshot snapshot{};
  snapshot.name = "dynamic_on_static";
  snapshot.bodies.push_back(captureBody(*world, box));

  const BodyState& state = snapshot.bodies[0];
  assert(state.pos_z > Fixed::from_int(1).raw());
  assert(state.vel_z == 0);
  assert(state.vel_x == 0);
  assert(state.vel_y == 0);

  static const BodyState kExpected[] = {
      {0, 0, 19017041450, 0, 0, 0, 4294967296, 0, 0, 0, 1},
  };
  assertScenarioMatches(snapshot, kExpected, 1);

  world->destroy();
  return snapshot;
}

ScenarioSnapshot scenario_stack_three_boxes() {
  PhysicsWorld* world = PhysicsWorld::create();

  makeFloor(*world);

  const Fixed half = Fixed::from_int(1);
  const FixedVec3 half_extents(half, half, half);

  PhysicsTransform pose1{};
  pose1.position.z = Fixed::from_int(3);
  const RigidBodyHandle box1 = world->createRigidBody(MotionType::Dynamic, pose1, Fixed::from_int(1));
  (void)world->attachBoxCollider(box1, half_extents);

  PhysicsTransform pose2{};
  pose2.position.z = Fixed::from_int(5);
  const RigidBodyHandle box2 = world->createRigidBody(MotionType::Dynamic, pose2, Fixed::from_int(1));
  (void)world->attachBoxCollider(box2, half_extents);

  PhysicsTransform pose3{};
  pose3.position.z = Fixed::from_int(7);
  const RigidBodyHandle box3 = world->createRigidBody(MotionType::Dynamic, pose3, Fixed::from_int(1));
  (void)world->attachBoxCollider(box3, half_extents);

  const Fixed dt = testDt();
  for (int i = 0; i < 360; ++i) {
    world->step(dt);
  }

  ScenarioSnapshot snapshot{};
  snapshot.name = "stack_three_boxes";
  snapshot.bodies.push_back(captureBody(*world, box1));
  snapshot.bodies.push_back(captureBody(*world, box2));
  snapshot.bodies.push_back(captureBody(*world, box3));

  for (const BodyState& state : snapshot.bodies) {
    assert(state.vel_z == 0);
    assert(state.vel_x == 0);
    assert(state.vel_y == 0);
  }
  assert(snapshot.bodies[0].pos_z < snapshot.bodies[1].pos_z);
  assert(snapshot.bodies[1].pos_z < snapshot.bodies[2].pos_z);

  static const BodyState kExpected[] = {
      {0, 0, 10427106858, 0, 0, 0, 4294967296, 0, 0, 0, 1},
      {0, 0, 19017041450, 0, 0, 0, 4294967296, 0, 0, 0, 1},
      {0, 0, 27606976042, 0, 0, 0, 4294967296, 0, 0, 0, 1},
  };
  assertScenarioMatches(snapshot, kExpected, 3);

  world->destroy();
  return snapshot;
}

ScenarioSnapshot scenario_sphere_and_capsule_resting() {
  PhysicsWorld* world = PhysicsWorld::create();

  makeFloor(*world);

  PhysicsTransform sphere_pose{};
  sphere_pose.position.x = Fixed::from_int(-3);
  sphere_pose.position.z = Fixed::from_int(6);
  const RigidBodyHandle sphere = world->createRigidBody(MotionType::Dynamic, sphere_pose, Fixed::from_int(1));
  (void)world->attachSphereCollider(sphere, Fixed::from_int(1));

  PhysicsTransform capsule_pose{};
  capsule_pose.position.x = Fixed::from_int(3);
  capsule_pose.position.z = Fixed::from_int(8);
  const RigidBodyHandle capsule =
      world->createRigidBody(MotionType::Dynamic, capsule_pose, Fixed::from_int(1));
  (void)world->attachCapsuleCollider(capsule, Fixed::from_int(1), Fixed::from_int(1));

  const Fixed dt = testDt();
  for (int i = 0; i < 300; ++i) {
    world->step(dt);
  }

  ScenarioSnapshot snapshot{};
  snapshot.name = "sphere_and_capsule_resting";
  snapshot.bodies.push_back(captureBody(*world, sphere));
  snapshot.bodies.push_back(captureBody(*world, capsule));

  for (const BodyState& state : snapshot.bodies) {
    assert(state.vel_z == 0);
    assert(state.vel_x == 0);
    assert(state.vel_y == 0);
    assert(state.pos_z > Fixed::from_int(1).raw());
  }

  static const BodyState kExpected[] = {
      {-12884901888, 0, 23312008746, 0, 0, 0, 4294967296, 0, 0, 0, 1},
      {12884901888, 0, 31901943338, 0, 0, 0, 4294967296, 0, 0, 0, 1},
  };
  assertScenarioMatches(snapshot, kExpected, 2);

  world->destroy();
  return snapshot;
}

ScenarioSnapshot scenario_frictional_incline() {
  PhysicsWorld* world = PhysicsWorld::create();
  world->setGravity(FixedVec3(Fixed::from_int(-3), Fixed::zero(), -(Fixed::from_int(981) / Fixed::from_int(100))));

  PhysicsMaterial high_friction{};
  high_friction.friction = Fixed::from_int(1) / Fixed::from_int(2);

  PhysicsTransform floor_pose{};
  const RigidBodyHandle floor = world->createRigidBody(MotionType::Static, floor_pose, Fixed::zero());
  (void)world->attachBoxCollider(floor, FixedVec3(Fixed::from_int(10), Fixed::from_int(10), Fixed::from_int(1)),
                           high_friction);

  PhysicsTransform box_pose{};
  box_pose.position.z = Fixed::from_int(3);
  const RigidBodyHandle box = world->createRigidBody(MotionType::Dynamic, box_pose, Fixed::from_int(1));
  (void)world->attachBoxCollider(box, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)),
                           high_friction);

  const Fixed dt = testDt();
  for (int i = 0; i < 240; ++i) {
    world->step(dt);
  }

  ScenarioSnapshot snapshot{};
  snapshot.name = "frictional_incline";
  snapshot.bodies.push_back(captureBody(*world, box));

  const BodyState& state = snapshot.bodies[0];
  const Fixed speed_sq =
      Fixed::from_raw(state.vel_x) * Fixed::from_raw(state.vel_x) +
      Fixed::from_raw(state.vel_y) * Fixed::from_raw(state.vel_y) +
      Fixed::from_raw(state.vel_z) * Fixed::from_raw(state.vel_z);
  const Fixed threshold = Fixed::from_int(1) / Fixed::from_int(5);
  assert(speed_sq.raw() <= threshold.raw() * threshold.raw());

  static const BodyState kExpected[] = {
      {-751619281, 0, 10427106858, 0, 0, 0, 4294967296, 0, 0, 0, 1},
  };
  assertScenarioMatches(snapshot, kExpected, 1);

  world->destroy();
  return snapshot;
}

ScenarioSnapshot scenario_kinematic_platform() {
  PhysicsWorld* world = PhysicsWorld::create();
  world->setGravity(FixedVec3{});

  PhysicsTransform platform_pose{};
  platform_pose.position.z = Fixed::from_int(2);
  const RigidBodyHandle platform = world->createRigidBody(MotionType::Kinematic, platform_pose, Fixed::zero());
  (void)world->attachBoxCollider(platform, FixedVec3(Fixed::from_int(2), Fixed::from_int(2), Fixed::from_int(1)));

  PhysicsTransform box_pose{};
  box_pose.position.z = Fixed::from_int(4);
  const RigidBodyHandle box = world->createRigidBody(MotionType::Dynamic, box_pose, Fixed::from_int(1));
  (void)world->attachBoxCollider(box, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));

  const Fixed dt = testDt();
  for (int i = 0; i < 30; ++i) {
    world->step(dt);
  }

  for (int i = 0; i < 60; ++i) {
    PhysicsTransform target = platform_pose;
    target.position.z =
        platform_pose.position.z + (Fixed::from_int(i + 1) * (Fixed::from_int(2) / Fixed::from_int(60)));
    world->setKinematicTarget(platform, target);
    world->step(dt);
  }

  ScenarioSnapshot snapshot{};
  snapshot.name = "kinematic_platform";
  snapshot.bodies.push_back(captureBody(*world, box));

  assert(snapshot.bodies[0].pos_z > Fixed::from_int(4).raw());

  static const BodyState kExpected[] = {
      {0, 0, 25769803742, 0, 0, 0, 4294967296, 0, 0, 8589934592, 0},
  };
  assertScenarioMatches(snapshot, kExpected, 1);

  world->destroy();
  return snapshot;
}

ScenarioSnapshot scenario_impact_energy_bound() {
  PhysicsWorld* world = PhysicsWorld::create();

  PhysicsMaterial no_bounce{};
  no_bounce.restitution = Fixed::zero();

  PhysicsTransform floor_pose{};
  const RigidBodyHandle floor = world->createRigidBody(MotionType::Static, floor_pose, Fixed::zero());
  (void)world->attachBoxCollider(floor, FixedVec3(Fixed::from_int(10), Fixed::from_int(10), Fixed::from_int(1)),
                           no_bounce);

  PhysicsTransform box_pose{};
  box_pose.position.z = Fixed::from_int(10);
  const RigidBodyHandle box = world->createRigidBody(MotionType::Dynamic, box_pose, Fixed::from_int(2));
  (void)world->attachBoxCollider(box, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)),
                           no_bounce);

  const Fixed dt = testDt();
  const Fixed mass = Fixed::from_int(2);
  int64_t peak_ke_raw = 0;

  for (int i = 0; i < 300; ++i) {
    world->step(dt);
    const FixedVec3 velocity = world->getLinearVelocity(box);
    const Fixed ke =
        (mass * (velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z)) /
        Fixed::from_int(2);
    if (ke.raw() > peak_ke_raw) {
      peak_ke_raw = ke.raw();
    }
  }

  ScenarioSnapshot snapshot{};
  snapshot.name = "impact_energy_bound";
  snapshot.bodies.push_back(captureBody(*world, box));

  const BodyState& settled = snapshot.bodies[0];
  const Fixed settled_ke =
      (mass * (Fixed::from_raw(settled.vel_x) * Fixed::from_raw(settled.vel_x) +
              Fixed::from_raw(settled.vel_y) * Fixed::from_raw(settled.vel_y) +
              Fixed::from_raw(settled.vel_z) * Fixed::from_raw(settled.vel_z))) /
      Fixed::from_int(2);

  const Fixed energy_bound = Fixed::from_int(1) / Fixed::from_int(10);
  assert(settled_ke.raw() <= energy_bound.raw());
  assert(settled.vel_z == 0);
  assert(settled.vel_x == 0);
  assert(settled.vel_y == 0);
  assert(peak_ke_raw > settled_ke.raw());

  static const BodyState kExpected[] = {
      {0, 0, 40491877930, 0, 0, 0, 4294967296, 0, 0, 0, 1},
  };
  assertScenarioMatches(snapshot, kExpected, 1);

  world->destroy();
  return snapshot;
}

ScenarioSnapshot scenario_sleep_wake_on_hit() {
  PhysicsWorld* world = PhysicsWorld::create();

  makeFloor(*world);

  PhysicsTransform box_pose{};
  box_pose.position.z = Fixed::from_int(3);
  const RigidBodyHandle box = world->createRigidBody(MotionType::Dynamic, box_pose, Fixed::from_int(1));
  (void)world->attachBoxCollider(box, FixedVec3(Fixed::from_int(1), Fixed::from_int(1), Fixed::from_int(1)));

  const Fixed dt = testDt();
  for (int i = 0; i < 480; ++i) {
    world->step(dt);
  }
  assert(world->isBodySleeping(box));

  world->applyImpulse(box, FixedVec3(Fixed::from_int(1), Fixed::zero(), Fixed::zero()));
  world->step(dt);
  assert(!world->isBodySleeping(box));

  ScenarioSnapshot snapshot{};
  snapshot.name = "sleep_wake_on_hit";
  snapshot.bodies.push_back(captureBody(*world, box));
  assert(snapshot.bodies[0].sleeping == 0);
  assert(snapshot.bodies[0].vel_x > 0);

  static const BodyState kExpected[] = {
      {71582788, 0, 10415403072, 0, 0, 0, 4294967296, 4294967296, 0, -702227151, 0},
  };
  assertScenarioMatches(snapshot, kExpected, 1);

  world->destroy();
  return snapshot;
}

std::vector<ScenarioSnapshot> runAllScenarios() {
  std::vector<ScenarioSnapshot> snapshots;
  snapshots.push_back(scenario_free_fall());
  snapshots.push_back(scenario_dynamic_on_static());
  snapshots.push_back(scenario_stack_three_boxes());
  snapshots.push_back(scenario_sphere_and_capsule_resting());
  snapshots.push_back(scenario_frictional_incline());
  snapshots.push_back(scenario_kinematic_platform());
  snapshots.push_back(scenario_impact_energy_bound());
  snapshots.push_back(scenario_sleep_wake_on_hit());
  return snapshots;
}

}  // namespace

int main(int argc, char** argv) {
  const char* dump_path = nullptr;
  const char* compare_path = nullptr;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
      dump_path = argv[++i];
    } else if (std::strcmp(argv[i], "--compare") == 0 && i + 1 < argc) {
      compare_path = argv[++i];
    }
  }

  const std::vector<ScenarioSnapshot> snapshots = runAllScenarios();

  if (dump_path) {
    if (!writeDumpFile(dump_path, snapshots)) {
      std::fprintf(stderr, "physics_golden_suite: failed to write dump '%s'\n", dump_path);
      return 1;
    }
  }

  if (compare_path) {
    if (!compareDumpFile(compare_path, snapshots, stderr)) {
      return 1;
    }
  }

  return 0;
}
