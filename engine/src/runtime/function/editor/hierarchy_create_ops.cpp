#include "runtime/function/editor/hierarchy_create_ops.h"

#include <cstdio>

#include <glm/gtc/quaternion.hpp>

#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/light_component.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {

bool parseHierarchyCreateKind(const eastl::string& name, HierarchyCreateKind& out_kind) {
  if (name == "empty") {
    out_kind = HierarchyCreateKind::empty;
    return true;
  }
  if (name == "camera") {
    out_kind = HierarchyCreateKind::camera;
    return true;
  }
  if (name == "light") {
    out_kind = HierarchyCreateKind::light;
    return true;
  }
  return false;
}

const char* hierarchyCreateKindBaseName(HierarchyCreateKind kind) {
  switch (kind) {
    case HierarchyCreateKind::camera:
      return "Camera";
    case HierarchyCreateKind::light:
      return "Light";
    case HierarchyCreateKind::empty:
    default:
      return "Empty";
  }
}

eastl::string hierarchyCreateCommandLabel(const eastl::string& entity_name) {
  if (entity_name.empty()) {
    return eastl::string("Create Entity");
  }
  return eastl::string("Create ") + entity_name;
}

namespace {

bool nameTakenInDocument(const SceneInstance& scene, const eastl::string& name) {
  const EntityId id = scene.findEntityByName(name);
  if (!isValid(id)) {
    return false;
  }
  return !scene.isOmittedFromDocument(id);
}

}  // namespace

eastl::string uniqueHierarchyCreateName(const SceneInstance& scene,
                                        const eastl::string& base) {
  if (!nameTakenInDocument(scene, base)) {
    return base;
  }
  for (uint32_t index = 1; index < 10000; ++index) {
    char candidate[128];
    std::snprintf(candidate, sizeof(candidate), "%s_%u", base.c_str(), index);
    const eastl::string name(candidate);
    if (!nameTakenInDocument(scene, name)) {
      return name;
    }
  }
  return base;
}

HierarchyCreateResult applyHierarchyCreate(SceneInstance* scene, EntityId parent_id,
                                           HierarchyCreateKind kind) {
  HierarchyCreateResult result{};
  if (scene == nullptr) {
    return result;
  }

  EntityId resolved_parent = k_invalid_entity_id;
  if (isValid(parent_id)) {
    if (scene->getEntity(parent_id) == nullptr ||
        scene->isOmittedFromDocument(parent_id)) {
      return result;
    }
    resolved_parent = parent_id;
  }

  const eastl::string name =
      uniqueHierarchyCreateName(*scene, hierarchyCreateKindBaseName(kind));
  const EntityId entity_id = scene->createEntity(
      name, Vec3(0.0f), glm::identity<Quat>(), Vec3(1.0f), resolved_parent);
  if (!isValid(entity_id) || scene->getEntity(entity_id) == nullptr) {
    return result;
  }

  if (kind == HierarchyCreateKind::camera) {
    CameraComponent camera{};
    camera.is_main = false;
    scene->setCamera(entity_id, camera);
  } else if (kind == HierarchyCreateKind::light) {
    scene->setLight(entity_id, LightComponent{});
  }

  result.entity_id = entity_id;
  result.created = true;
  return result;
}

}  // namespace Blunder
