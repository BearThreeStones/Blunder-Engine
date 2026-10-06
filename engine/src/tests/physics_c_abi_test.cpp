#include "runtime/core/log/log_system.h"
#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/core/reflection/class_db.h"
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

bool name_is(BlunderObjectId id, const char* expected) {
  char name[128]{};
  return blunder_object_get_name(id, name, static_cast<int>(sizeof(name))) ==
             BLUNDER_ENGINE_OK &&
         std::strcmp(name, expected) == 0;
}

}  // namespace

int main() {
  using namespace Blunder;
  if (!g_runtime_global_context.m_logger_system) {
    g_runtime_global_context.m_logger_system = eastl::make_shared<LogSystem>();
  }
  ObjectDB::clear();
  ClassDB::initialize();

  expect_true("abi version 15", blunder_engine_abi_version() == 15);
  expect_true("header version 15", BLUNDER_ENGINE_C_ABI_VERSION == 15);

  auto scenes = eastl::make_shared<SceneSystem>();
  g_runtime_global_context.m_scene_system = scenes;
  g_runtime_global_context.m_physics_manager = eastl::make_shared<PhysicsManager>();

  SceneInstance scene;
  scenes->setActiveInstance(&scene);

  const EntityId ice_id =
      scene.createEntity("Ice", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1));
  ColliderComponent ice{};
  ice.box_half_extents = Vec3(1.0f, 1.0f, 1.0f);
  scene.setCollider(ice_id, ice);
  scene.addGroup(ice_id, "TerrainIce");
  expect_true("ice starts unbound", scene.findBoundObject(ice_id) == nullptr);

  const EntityId root_id =
      scene.createEntity("Root", Vec3(10, 0, 1), glm::identity<Quat>(), Vec3(1));
  const EntityId ghost_id = scene.createEntity(
      "Ghost", Vec3(0, 0, 0), glm::identity<Quat>(), Vec3(1), root_id);
  scene.addGroup(ghost_id, "TerrainIce");
  const EntityId pivot_id = scene.createEntity(
      "Pivot", Vec3(0, 2, 3), glm::identity<Quat>(), Vec3(1), root_id);
  scene.createEntity("Grand", Vec3(1, 0, 0), glm::identity<Quat>(), Vec3(1),
                     pivot_id);

  const EntityId sleeper_id =
      scene.createEntity("Sleeper", Vec3(4, 0, 0), glm::identity<Quat>(), Vec3(1));
  scene.setObjectActive(sleeper_id, false);

  const EntityId twin_first =
      scene.createEntity("Twin", Vec3(1, 0, 0), glm::identity<Quat>(), Vec3(1));
  Object* twin_first_object = scene.ensureBoundObject(twin_first);
  const BlunderObjectId twin_first_abi =
      twin_first_object != nullptr
          ? static_cast<BlunderObjectId>(twin_first_object->getId())
          : 0;

  scene.createEntity("Twin", Vec3(2, 0, 0), glm::identity<Quat>(), Vec3(1));

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
  Object* ice_object = scene.findBoundObject(ice_id);
  expect_true("raycast lazy bound ice",
              ice_object != nullptr && hit.object_id != 0 &&
                  hit.object_id == static_cast<BlunderObjectId>(ice_object->getId()));

  BlunderObjectId ghost_abi = 0;
  expect_true("find ghost before tombstone",
              blunder_scene_find_object("Ghost", &ghost_abi) == BLUNDER_ENGINE_OK &&
                  ghost_abi != 0);
  expect_true("tombstone ghost", scene.softDeleteEntity(ghost_id));
  BlunderObjectId ghost_after = 1;
  expect_true("find skips tombstone",
              blunder_scene_find_object("Ghost", &ghost_after) == BLUNDER_ENGINE_ERROR &&
                  ghost_after == 0);

  int ice_found = 0;
  BlunderObjectId ice_ids[8]{};
  expect_true("find TerrainIce",
              blunder_find_objects_in_group("TerrainIce", ice_ids, 8, &ice_found) ==
                      BLUNDER_ENGINE_OK &&
                  ice_found == 1 && ice_ids[0] == hit.object_id);
  int ice_in_group = 0;
  expect_true("ice is in TerrainIce",
              blunder_object_is_in_group(ice_ids[0], "TerrainIce", &ice_in_group) ==
                      BLUNDER_ENGINE_OK &&
                  ice_in_group == 1);
  bool group_skipped_ghost = true;
  for (int i = 0; i < ice_found; ++i) {
    if (ice_ids[i] == ghost_abi) {
      group_skipped_ghost = false;
    }
  }
  expect_true("group skips tombstone", group_skipped_ghost);

  expect_true("pivot starts unbound", scene.findBoundObject(pivot_id) == nullptr);
  BlunderObjectId pivot_abi = 0;
  expect_true("find pivot",
              blunder_scene_find_object("Pivot", &pivot_abi) == BLUNDER_ENGINE_OK &&
                  pivot_abi != 0 && name_is(pivot_abi, "Pivot"));
  expect_true("find bound pivot", scene.findBoundObject(pivot_id) != nullptr);
  BlunderObjectId root_abi = 0;
  expect_true("pivot parent",
              blunder_object_get_parent(pivot_abi, &root_abi) == BLUNDER_ENGINE_OK &&
                  root_abi != 0 && name_is(root_abi, "Root"));
  BlunderObjectId root_parent = 1;
  expect_true("root has no parent",
              blunder_object_get_parent(root_abi, &root_parent) == BLUNDER_ENGINE_OK &&
                  root_parent == 0);
  expect_true("root child count", blunder_object_child_count(root_abi) == 1);
  BlunderObjectId root_child = 0;
  expect_true("root child is pivot",
              blunder_object_child_at(root_abi, 0, &root_child) == BLUNDER_ENGINE_OK &&
                  root_child == pivot_abi);
  expect_true("pivot child count", blunder_object_child_count(pivot_abi) == 1);
  BlunderObjectId grand_abi = 0;
  expect_true("pivot child is grand",
              blunder_object_child_at(pivot_abi, 0, &grand_abi) == BLUNDER_ENGINE_OK &&
                  name_is(grand_abi, "Grand"));

  float wx = 0, wy = 0, wz = 0;
  expect_true("pivot world position",
              blunder_object_get_world_position(pivot_abi, &wx, &wy, &wz) ==
                      BLUNDER_ENGINE_OK &&
                  float_near(wx, 10.0f, 0.001f) && float_near(wy, 2.0f, 0.001f) &&
                  float_near(wz, 4.0f, 0.001f));
  float lx = 0, ly = 0, lz = 0;
  expect_true("pivot position stays local",
              blunder_object_get_vec3_property(pivot_abi, "Object", "position", &lx,
                                              &ly, &lz) == BLUNDER_ENGINE_OK &&
                  float_near(lx, 0.0f, 0.001f) && float_near(ly, 2.0f, 0.001f) &&
                  float_near(lz, 3.0f, 0.001f));

  BlunderObjectId twin_abi = 0;
  expect_true("duplicate name last wins",
              blunder_scene_find_object("Twin", &twin_abi) == BLUNDER_ENGINE_OK &&
                  twin_abi != 0 && twin_abi != twin_first_abi);
  float tx = 0, ty = 0, tz = 0;
  expect_true("duplicate find is the later twin",
              blunder_object_get_vec3_property(twin_abi, "Object", "position", &tx, &ty,
                                              &tz) == BLUNDER_ENGINE_OK &&
                  float_near(tx, 2.0f, 0.001f));

  BlunderObjectId sleeper_abi = 0;
  expect_true("inactive still findable",
              blunder_scene_find_object("Sleeper", &sleeper_abi) ==
                      BLUNDER_ENGINE_OK &&
                  sleeper_abi != 0 && name_is(sleeper_abi, "Sleeper"));

  BlunderObjectId missing = 1;
  expect_true("missing name",
              blunder_scene_find_object("Nope", &missing) == BLUNDER_ENGINE_ERROR &&
                  missing == 0);
  expect_true("empty name",
              blunder_scene_find_object("", &missing) == BLUNDER_ENGINE_ERROR);

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
  BlunderObjectId no_scene = 1;
  expect_true("find without active scene",
              blunder_scene_find_object("Pivot", &no_scene) == BLUNDER_ENGINE_ERROR &&
                  no_scene == 0);

  g_runtime_global_context.m_physics_manager.reset();
  g_runtime_global_context.m_scene_system.reset();
  ClassDB::shutdown();
  ObjectDB::clear();
  g_runtime_global_context.m_logger_system.reset();

  if (g_failures != 0) {
    std::fprintf(stderr, "%d physics_c_abi_test failure(s)\n", g_failures);
    return 1;
  }
  std::printf("physics_c_abi_test: all passed\n");
  return 0;
}
