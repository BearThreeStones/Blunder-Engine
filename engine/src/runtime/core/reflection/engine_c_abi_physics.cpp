#include "runtime/core/reflection/engine_c_abi.h"

#include "runtime/core/object/object.h"
#include "runtime/core/object/object_db.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/physics/physics_manager.h"
#include "runtime/function/scene/character_controller_component.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include <cstring>

using namespace Blunder;

namespace {

SceneInstance* activeScene() {
  if (!g_runtime_global_context.m_scene_system) {
    return nullptr;
  }
  return g_runtime_global_context.m_scene_system->getActiveInstance();
}

PhysicsManager* physicsManager() {
  return g_runtime_global_context.m_physics_manager.get();
}

SceneInstance* sceneForObject(Object* object) {
  SceneInstance* scene = activeScene();
  if (scene == nullptr || object == nullptr || !object->hasEntity()) {
    return nullptr;
  }
  if (scene->getEntity(object->getEntityId()) == nullptr) {
    return nullptr;
  }
  return scene;
}

void writeGroupsCsv(const eastl::vector<eastl::string>& groups, char* out,
                    size_t capacity) {
  if (out == nullptr || capacity == 0) {
    return;
  }
  out[0] = '\0';
  size_t used = 0;
  for (size_t i = 0; i < groups.size(); ++i) {
    if (i > 0) {
      if (used + 2 >= capacity) {
        break;
      }
      out[used++] = ',';
      out[used] = '\0';
    }
    const eastl::string& group = groups[i];
    if (used + group.size() + 1 >= capacity) {
      break;
    }
    std::memcpy(out + used, group.c_str(), group.size());
    used += group.size();
    out[used] = '\0';
  }
}

void fillCAbiHit(const PhysicsSceneHit& src, BlunderPhysicsHit* out_hit) {
  if (out_hit == nullptr) {
    return;
  }
  std::memset(out_hit, 0, sizeof(*out_hit));
  out_hit->hit = src.hit ? 1 : 0;
  out_hit->is_area = src.is_area ? 1 : 0;
  out_hit->distance = src.distance;
  out_hit->point_x = src.point.x;
  out_hit->point_y = src.point.y;
  out_hit->point_z = src.point.z;
  out_hit->normal_x = src.normal.x;
  out_hit->normal_y = src.normal.y;
  out_hit->normal_z = src.normal.z;
  out_hit->object_id = src.object_id;
  writeGroupsCsv(src.groups, out_hit->groups, sizeof(out_hit->groups));
}

CharacterControllerComponent* controllerForObject(Object* object,
                                                  SceneInstance** out_scene) {
  SceneInstance* scene = sceneForObject(object);
  if (out_scene != nullptr) {
    *out_scene = scene;
  }
  if (scene == nullptr) {
    return nullptr;
  }
  return scene->getCharacterController(object->getEntityId());
}

int writeCString(const eastl::string& value, char* out_name, int name_capacity) {
  if (out_name == nullptr || name_capacity <= 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  const int max_copy = name_capacity - 1;
  const int n = static_cast<int>(value.size()) < max_copy
                    ? static_cast<int>(value.size())
                    : max_copy;
  if (n > 0) {
    std::memcpy(out_name, value.c_str(), static_cast<size_t>(n));
  }
  out_name[n] = '\0';
  return BLUNDER_ENGINE_OK;
}

}  // namespace

extern "C" {

int blunder_physics_raycast(const BlunderPhysicsRay* ray, BlunderPhysicsHit* out_hit) {
  if (ray == nullptr || out_hit == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  SceneInstance* scene = activeScene();
  PhysicsManager* physics = physicsManager();
  if (scene == nullptr || physics == nullptr) {
    std::memset(out_hit, 0, sizeof(*out_hit));
    return BLUNDER_ENGINE_ERROR;
  }
  PhysicsSceneHit hit{};
  const bool ok = physics->raycast(
      *scene, Vec3(ray->ox, ray->oy, ray->oz), Vec3(ray->dx, ray->dy, ray->dz),
      ray->max_distance, ray->mask, ray->collide_with_areas != 0, hit);
  fillCAbiHit(hit, out_hit);
  return ok && hit.hit ? BLUNDER_ENGINE_OK : BLUNDER_ENGINE_ERROR;
}

int blunder_physics_shapecast(const BlunderPhysicsSweep* sweep,
                              BlunderPhysicsHit* out_hit) {
  if (sweep == nullptr || out_hit == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  SceneInstance* scene = activeScene();
  PhysicsManager* physics = physicsManager();
  if (scene == nullptr || physics == nullptr) {
    std::memset(out_hit, 0, sizeof(*out_hit));
    return BLUNDER_ENGINE_ERROR;
  }
  PhysicsSweepShape shape = PhysicsSweepShape::Box;
  if (sweep->shape == BLUNDER_PHYSICS_SWEEP_SPHERE) {
    shape = PhysicsSweepShape::Sphere;
  } else if (sweep->shape == BLUNDER_PHYSICS_SWEEP_CAPSULE) {
    shape = PhysicsSweepShape::Capsule;
  } else if (sweep->shape != BLUNDER_PHYSICS_SWEEP_BOX) {
    std::memset(out_hit, 0, sizeof(*out_hit));
    return BLUNDER_ENGINE_ERROR;
  }
  PhysicsSceneHit hit{};
  const bool ok = physics->shapecast(
      *scene, shape, Vec3(sweep->ox, sweep->oy, sweep->oz),
      Quat(sweep->qw, sweep->qx, sweep->qy, sweep->qz),
      Vec3(sweep->hx, sweep->hy, sweep->hz), sweep->sphere_radius,
      sweep->capsule_radius, sweep->capsule_half_height,
      Vec3(sweep->dx, sweep->dy, sweep->dz), sweep->max_distance, sweep->mask,
      sweep->collide_with_areas != 0, hit);
  fillCAbiHit(hit, out_hit);
  return ok && hit.hit ? BLUNDER_ENGINE_OK : BLUNDER_ENGINE_ERROR;
}

int blunder_object_add_group(BlunderObjectId id, const char* name) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  SceneInstance* scene = sceneForObject(object);
  if (scene == nullptr || name == nullptr || name[0] == '\0') {
    return BLUNDER_ENGINE_ERROR;
  }
  scene->addGroup(object->getEntityId(), eastl::string(name));
  return BLUNDER_ENGINE_OK;
}

int blunder_object_remove_group(BlunderObjectId id, const char* name) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  SceneInstance* scene = sceneForObject(object);
  if (scene == nullptr || name == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  scene->removeGroup(object->getEntityId(), eastl::string(name));
  return BLUNDER_ENGINE_OK;
}

int blunder_object_is_in_group(BlunderObjectId id, const char* name, int* out_value) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  SceneInstance* scene = sceneForObject(object);
  if (scene == nullptr || name == nullptr || out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_value = scene->isInGroup(object->getEntityId(), eastl::string(name)) ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

int blunder_object_group_count(BlunderObjectId id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  SceneInstance* scene = sceneForObject(object);
  if (scene == nullptr) {
    return 0;
  }
  return static_cast<int>(scene->getGroups(object->getEntityId()).size());
}

int blunder_object_group_at(BlunderObjectId id, int index, char* out_name,
                            int name_capacity) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  SceneInstance* scene = sceneForObject(object);
  if (scene == nullptr || index < 0) {
    return BLUNDER_ENGINE_ERROR;
  }
  const eastl::vector<eastl::string>& groups = scene->getGroups(object->getEntityId());
  if (static_cast<size_t>(index) >= groups.size()) {
    return BLUNDER_ENGINE_ERROR;
  }
  return writeCString(groups[static_cast<size_t>(index)], out_name, name_capacity);
}

int blunder_find_objects_in_group(const char* name, BlunderObjectId* out_ids,
                                  int capacity, int* out_count) {
  SceneInstance* scene = activeScene();
  if (scene == nullptr || name == nullptr || out_count == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  eastl::vector<Object*> objects;
  scene->findBoundObjectsInGroup(eastl::string(name), objects);
  *out_count = static_cast<int>(objects.size());
  if (out_ids == nullptr || capacity <= 0) {
    return BLUNDER_ENGINE_OK;
  }
  const int n = *out_count < capacity ? *out_count : capacity;
  for (int i = 0; i < n; ++i) {
    out_ids[i] = static_cast<BlunderObjectId>(objects[static_cast<size_t>(i)]->getId());
  }
  return BLUNDER_ENGINE_OK;
}

int blunder_object_has_character_controller(BlunderObjectId id, int* out_value) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  if (out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  SceneInstance* scene = nullptr;
  *out_value = controllerForObject(object, &scene) != nullptr ? 1 : 0;
  return object != nullptr && scene != nullptr ? BLUNDER_ENGINE_OK
                                               : BLUNDER_ENGINE_ERROR;
}

int blunder_character_controller_move_and_slide(BlunderObjectId id) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  SceneInstance* scene = nullptr;
  CharacterControllerComponent* controller = controllerForObject(object, &scene);
  PhysicsManager* physics = physicsManager();
  if (controller == nullptr || scene == nullptr || physics == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  const Vec3 displacement = controller->velocity * (1.0f / 60.0f);
  return physics->moveAndSlide(*scene, object->getEntityId(), displacement)
             ? BLUNDER_ENGINE_OK
             : BLUNDER_ENGINE_ERROR;
}

int blunder_character_controller_set_velocity(BlunderObjectId id, float x, float y,
                                              float z) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  CharacterControllerComponent* controller = controllerForObject(object, nullptr);
  if (controller == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  controller->velocity = Vec3(x, y, z);
  return BLUNDER_ENGINE_OK;
}

int blunder_character_controller_get_velocity(BlunderObjectId id, float* x, float* y,
                                              float* z) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  CharacterControllerComponent* controller = controllerForObject(object, nullptr);
  if (controller == nullptr || x == nullptr || y == nullptr || z == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *x = controller->velocity.x;
  *y = controller->velocity.y;
  *z = controller->velocity.z;
  return BLUNDER_ENGINE_OK;
}

int blunder_character_controller_is_on_floor(BlunderObjectId id, int* out_value) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  CharacterControllerComponent* controller = controllerForObject(object, nullptr);
  if (controller == nullptr || out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_value = controller->on_floor ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

int blunder_character_controller_is_on_wall(BlunderObjectId id, int* out_value) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  CharacterControllerComponent* controller = controllerForObject(object, nullptr);
  if (controller == nullptr || out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_value = controller->on_wall ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

int blunder_character_controller_is_on_ceiling(BlunderObjectId id, int* out_value) {
  Object* object = ObjectDB::get(static_cast<ObjectId>(id));
  CharacterControllerComponent* controller = controllerForObject(object, nullptr);
  if (controller == nullptr || out_value == nullptr) {
    return BLUNDER_ENGINE_ERROR;
  }
  *out_value = controller->on_ceiling ? 1 : 0;
  return BLUNDER_ENGINE_OK;
}

}  // extern "C"
