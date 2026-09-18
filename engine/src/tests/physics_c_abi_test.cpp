#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/engine_c_abi.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/collider_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int g_failures = 0;

void expect_true(const char* label, bool ok) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", label);
    ++g_failures;
  }
}

bool float_near(float a, float b, float eps = 0.1f) {
  return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
  ObjectDB::clear();

  expect_true("abi version >= 13", blunder_engine_abi_version() >= 13);
  expect_true("header version 13", BLUNDER_ENGINE_C_ABI_VERSION >= 13);

  auto scenes = eastl::make_shared<SceneSystem>();
  g_runtime_global_context.m_scene_system = scenes;
  g_runtime_global_context.m_physics_manager = eastl::make_shared<PhysicsManager>();

  SceneInstance scene;
  scenes->setActiveInstance(&scene);

  const EntityId floor_id =
      scene.createEntity("Floor", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
  ColliderComponent floor{};
  floor.box_half_extents = Vec3(1.0f, 1.0f, 1.0f);
  scene.setCollider(floor_id, floor);
  scene.addGroup(floor_id, "TerrainIce");

  const EntityId walker_id =
      scene.createEntity("Walker", Vec3(0, 0, 4), glm::identity<Quat>(), Vec3(1));
  scene.setCharacterController(walker_id, CharacterControllerComponent{});
  Object* walker = scene.ensureBoundObject(walker_id);
  expect_true("walker object", walker != nullptr);
  const BlunderObjectId walker_abi =
      walker != nullptr ? static_cast<BlunderObjectId>(walker->getId()) : 0;

  BlunderPhysicsRay ray{};
  ray.oz = 5.0f;
  ray.dz = -1.0f;
  ray.max_distance = 20.0f;
  ray.mask = 0xFFFFFFFFu;
  BlunderPhysicsHit hit{};
  expect_true("c-abi raycast hit",
              blunder_physics_raycast(&ray, &hit) == BLUNDER_ENGINE_OK && hit.hit == 1);
  expect_true("c-abi hit metres", float_near(hit.distance, 4.0f));
  expect_true("c-abi groups csv", std::strstr(hit.groups, "TerrainIce") != nullptr);

  expect_true("add group",
              blunder_object_add_group(walker_abi, "Walkers") == BLUNDER_ENGINE_OK);
  int in_group = 0;
  expect_true("is in group",
              blunder_object_is_in_group(walker_abi, "Walkers", &in_group) ==
                      BLUNDER_ENGINE_OK &&
                  in_group == 1);
  expect_true("group count", blunder_object_group_count(walker_abi) == 1);
  char group_name[64]{};
  expect_true("group at",
              blunder_object_group_at(walker_abi, 0, group_name, 64) ==
                      BLUNDER_ENGINE_OK &&
                  std::strcmp(group_name, "Walkers") == 0);
  int found = 0;
  BlunderObjectId ids[4]{};
  expect_true("find objects",
              blunder_find_objects_in_group("Walkers", ids, 4, &found) ==
                      BLUNDER_ENGINE_OK &&
                  found == 1 && ids[0] == walker_abi);

  int has_cct = 0;
  expect_true("has cct",
              blunder_object_has_character_controller(walker_abi, &has_cct) ==
                      BLUNDER_ENGINE_OK &&
                  has_cct == 1);
  expect_true("set velocity",
              blunder_character_controller_set_velocity(walker_abi, 0.0f, 0.0f,
                                                        -1.0f) == BLUNDER_ENGINE_OK);
  float vx = 0, vy = 0, vz = 0;
  expect_true("get velocity",
              blunder_character_controller_get_velocity(walker_abi, &vx, &vy, &vz) ==
                      BLUNDER_ENGINE_OK &&
                  float_near(vz, -1.0f, 0.001f));
  expect_true("move and slide",
              blunder_character_controller_move_and_slide(walker_abi) ==
                  BLUNDER_ENGINE_OK);

  BlunderPhysicsRay miss{};
  miss.oz = 50.0f;
  miss.dz = 1.0f;
  miss.max_distance = 1.0f;
  miss.mask = 0xFFFFFFFFu;
  BlunderPhysicsHit miss_hit{};
  expect_true("c-abi miss is error",
              blunder_physics_raycast(&miss, &miss_hit) == BLUNDER_ENGINE_ERROR);

  scenes->setActiveInstance(nullptr);
  g_runtime_global_context.m_physics_manager.reset();
  g_runtime_global_context.m_scene_system.reset();
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d physics_c_abi_test failure(s)\n", g_failures);
    return 1;
  }
  std::printf("physics_c_abi_test: all passed\n");
  return 0;
}
