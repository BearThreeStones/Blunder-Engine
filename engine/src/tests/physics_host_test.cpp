#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/physics/physics_world.h"
#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/scene_instance.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 0.05f) {
  return std::fabs(a - b) <= eps;
}

void ensureLogger() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
}

}  // namespace

int main() {
  using namespace Blunder;
  ensureLogger();
  ObjectDB::clear();

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Box", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    ColliderComponent collider{};
    collider.shape = ColliderShapeKind::Box;
    collider.body_kind = ColliderBodyKind::Static;
    collider.box_half_extents = Vec3(1.0f, 1.0f, 1.0f);
    scene.setCollider(id, collider);

    PhysicsManager physics;
    expect_true("instantiate creates world", physics.ensureWorld(scene) != nullptr);
    PhysicsSceneHit hit{};
    expect_true("1m box ray hits",
                physics.raycast(scene, Vec3(0.0f, 0.0f, 5.0f),
                                Vec3(0.0f, 0.0f, -1.0f), 20.0f,
                                kDefaultColliderMask, false, hit) &&
                    hit.hit);
    expect_true("identity metres (not x100)", float_near(hit.distance, 4.0f, 0.1f));
  }

  {
    SceneInstance scene;
    const EntityId floor_id =
        scene.createEntity("Floor", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    ColliderComponent floor{};
    floor.box_half_extents = Vec3(8.0f, 8.0f, 0.25f);
    scene.setCollider(floor_id, floor);

    PhysicsManager physics;
    PhysicsWorld* world = physics.ensureWorld(scene);
    expect_true("play host world", world != nullptr);
    if (world != nullptr) {
      PhysicsTransform pose{};
      pose.position.z = Fixed::from_int(4);
      const RigidBodyHandle dynamic =
          world->createRigidBody(MotionType::Dynamic, pose, Fixed::from_int(1));
      world->attachSphereCollider(dynamic, Fixed::from_int(1));
      const int64_t z0 = world->getPose(dynamic).position.z.raw();
      physics.tick(1.0f / 60.0f, false, false);
      expect_true("edit does not step dynamics",
                  world->getPose(dynamic).position.z.raw() == z0);
      physics.tick(1.0f, true, true);
      expect_true("pause skips steps",
                  world->getPose(dynamic).position.z.raw() == z0);
      physics.tick(1.0f, true, false);
      expect_true("play steps dynamics",
                  world->getPose(dynamic).position.z.raw() < z0);
    }
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Moved", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    ColliderComponent collider{};
    collider.box_half_extents = Vec3(1.0f, 1.0f, 1.0f);
    scene.setCollider(id, collider);

    auto physics = eastl::make_shared<PhysicsManager>();
    g_runtime_global_context.m_physics_manager = physics;
    physics->ensureWorld(scene);
    PhysicsSceneHit origin_hit{};
    expect_true("query at origin",
                physics->raycast(scene, Vec3(0.0f, 0.0f, 5.0f),
                                 Vec3(0.0f, 0.0f, -1.0f), 20.0f,
                                 kDefaultColliderMask, false, origin_hit));
    scene.setTransform(id, Vec3(0.0f, 0.0f, 10.0f), glm::identity<Quat>(),
                       Vec3(1.0f));
    PhysicsSceneHit moved_hit{};
    expect_true("edit query after TRS",
                physics->raycast(scene, Vec3(0.0f, 0.0f, 15.0f),
                                 Vec3(0.0f, 0.0f, -1.0f), 20.0f,
                                 kDefaultColliderMask, false, moved_hit) &&
                    moved_hit.hit);
    PhysicsSceneHit stale{};
    expect_true("old pose miss after TRS",
                !physics->raycast(scene, Vec3(0.0f, 0.0f, 5.0f),
                                  Vec3(0.0f, 0.0f, -1.0f), 3.0f,
                                  kDefaultColliderMask, false, stale));
    g_runtime_global_context.m_physics_manager.reset();
  }

  {
    SceneInstance scene;
    const EntityId wall_id =
        scene.createEntity("Wall", Vec3(3, 0, 1), glm::identity<Quat>(), Vec3(1));
    ColliderComponent wall{};
    wall.box_half_extents = Vec3(0.5f, 4.0f, 4.0f);
    scene.setCollider(wall_id, wall);

    const EntityId walker_id =
        scene.createEntity("Walker", Vec3(0, 0, 1), glm::identity<Quat>(), Vec3(1));
    CharacterControllerComponent cct{};
    scene.setCharacterController(walker_id, cct);

    auto physics = eastl::make_shared<PhysicsManager>();
    g_runtime_global_context.m_physics_manager = physics;
    expect_true("cct world", physics->ensureWorld(scene) != nullptr);
    scene.setTransform(walker_id, Vec3(8.0f, 0.0f, 1.0f), glm::identity<Quat>(),
                       Vec3(1.0f));
    Vec3 pos{};
    Quat rot = glm::identity<Quat>();
    Vec3 scale{1.0f};
    expect_true("teleport reads back",
                scene.getTransform(walker_id, pos, rot, scale) &&
                    float_near(pos.x, 8.0f, 0.01f));

    scene.setTransform(walker_id, Vec3(0.0f, 0.0f, 1.0f), glm::identity<Quat>(),
                       Vec3(1.0f));
    expect_true("move and slide",
                physics->moveAndSlide(scene, walker_id, Vec3(10.0f, 0.0f, 0.0f)));
    expect_true("slide blocked by wall",
                scene.getTransform(walker_id, pos, rot, scale) && pos.x < 3.0f);
    const CharacterControllerComponent* after = scene.getCharacterController(walker_id);
    expect_true("on wall after slide", after != nullptr && after->on_wall);
    g_runtime_global_context.m_physics_manager.reset();
  }

  {
    SceneInstance scene;
    const EntityId floor_id =
        scene.createEntity("Floor", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    ColliderComponent floor{};
    floor.box_half_extents = Vec3(8.0f, 8.0f, 0.25f);
    scene.setCollider(floor_id, floor);

    const EntityId walker_id =
        scene.createEntity("Walker", Vec3(0, 0, 4), glm::identity<Quat>(), Vec3(1));
    CharacterControllerComponent cct{};
    scene.setCharacterController(walker_id, cct);

    PhysicsManager physics;
    expect_true("walker drop",
                physics.moveAndSlide(scene, walker_id, Vec3(0.0f, 0.0f, -10.0f)));
    Vec3 pos{};
    Quat rot = glm::identity<Quat>();
    Vec3 scale{1.0f};
    expect_true("walker rest transform",
                scene.getTransform(walker_id, pos, rot, scale));
    // Floor top 0.25 + height/2 0.9 + skin 0.04 = 1.19. Sphere-at-origin rest is ~0.69.
    expect_true("walker rest height/2", float_near(pos.z, 1.19f, 0.12f));
    const CharacterControllerComponent* after = scene.getCharacterController(walker_id);
    expect_true("walker on floor", after != nullptr && after->on_floor);
  }

  {
    auto physics = eastl::make_shared<PhysicsManager>();
    g_runtime_global_context.m_physics_manager = physics;
    {
      SceneInstance scene;
      const EntityId id =
          scene.createEntity("Box", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
      ColliderComponent collider{};
      collider.box_half_extents = Vec3(1.0f, 1.0f, 1.0f);
      scene.setCollider(id, collider);
      expect_true("bind before destroy", physics->ensureWorld(scene) != nullptr);
    }
    physics->tick(1.0f / 60.0f, true, false);
    expect_true("tick after scene destroy", true);
    g_runtime_global_context.m_physics_manager.reset();
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("Box", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    ColliderComponent collider{};
    collider.box_half_extents = Vec3(1.0f, 1.0f, 1.0f);
    scene.setCollider(id, collider);
    PhysicsManager physics;
    expect_true("unbind world created", physics.ensureWorld(scene) != nullptr);
    physics.unbind(&scene);
    expect_true("unbind drops world", physics.worldFor(&scene) == nullptr);
    physics.tick(1.0f / 60.0f, true, false);
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("KinematicMesh", Vec3(0, 0, 0), glm::identity<Quat>(),
                           Vec3(1));
    ColliderComponent mesh{};
    mesh.shape = ColliderShapeKind::TriangleMesh;
    mesh.body_kind = ColliderBodyKind::Kinematic;
    ColliderTriangle tri{};
    tri.v0 = Vec3(-2, -2, 0);
    tri.v1 = Vec3(2, -2, 0);
    tri.v2 = Vec3(0, 2, 0);
    mesh.triangles.push_back(tri);
    scene.setCollider(id, mesh);

    PhysicsManager physics;
    physics.ensureWorld(scene);
    PhysicsSceneHit hit{};
    expect_true("non-static trimesh skipped (no auto-box)",
                !physics.raycast(scene, Vec3(0.0f, 0.0f, 5.0f),
                                 Vec3(0.0f, 0.0f, -1.0f), 20.0f,
                                 kDefaultColliderMask, false, hit));
  }

  {
    SceneInstance scene;
    const EntityId id =
        scene.createEntity("EmptyMesh", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
    ColliderComponent mesh{};
    mesh.shape = ColliderShapeKind::TriangleMesh;
    scene.setCollider(id, mesh);
    PhysicsManager physics;
    physics.ensureWorld(scene);
    PhysicsSceneHit hit{};
    expect_true("empty trimesh skipped",
                !physics.raycast(scene, Vec3(0.0f, 0.0f, 5.0f),
                                 Vec3(0.0f, 0.0f, -1.0f), 20.0f,
                                 kDefaultColliderMask, false, hit));
  }

  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d physics_host_test failure(s)\n", g_failures);
    return 1;
  }
  std::printf("physics_host_test: all passed\n");
  return 0;
}
