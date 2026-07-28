#include "runtime/function/editor/align_camera_actions.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/editor/document_history_helpers.h"
#include "runtime/function/editor/editor_commands.h"
#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/gizmo/gizmo_math.h"
#include "runtime/function/scene/align_camera_target.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/play_camera_resolve.h"
#include "runtime/function/scene/scene_instance.h"

namespace Blunder {
namespace {

Vec3 localPositionForWorldPosition(const SceneInstance& scene,
                                   const Entity& entity,
                                   const Vec3& world_position) {
  Mat4 parent_world = scene.getSceneToWorldMatrix();
  const EntityId parent_id = entity.getParentId();
  if (isValid(parent_id)) {
    parent_world = scene.getWorldMatrix(parent_id);
  }
  const Vec4 local_position =
      glm::inverse(parent_world) * Vec4(world_position, 1.0f);
  return Vec3(local_position);
}

Quat worldRotationFromCameraDirections(const Vec3& view_forward,
                                       const Vec3& view_up) {
  Vec3 local_z = -glm::normalize(view_forward);
  Vec3 local_y = glm::normalize(view_up);
  if (std::abs(glm::dot(local_y, local_z)) > 0.95f) {
    local_y = kWorldUp;
  }
  local_y = glm::normalize(local_y - local_z * glm::dot(local_y, local_z));
  const Vec3 local_x = glm::normalize(glm::cross(local_y, local_z));
  const Mat3 basis(local_x, local_y, local_z);
  return glm::normalize(glm::quat_cast(Mat4(basis)));
}

Quat localRotationForWorldRotation(SceneInstance& scene, EntityId entity_id,
                                   const Quat& world_rotation) {
  const Quat parent_world_rotation = parentWorldRotation(scene, entity_id);
  return glm::normalize(glm::inverse(parent_world_rotation) * world_rotation);
}

}  // namespace

bool alignViewToCamera(EditorCamera& editor_camera, const SceneInstance& scene,
                       eastl::span<const EntityId> selected_ids) {
  const AlignCameraTargetResult target =
      resolveAlignCameraTarget(scene, selected_ids);
  if (!target.ok) {
    return false;
  }

  const CameraComponent* camera = scene.getCamera(target.entity_id);
  if (camera == nullptr) {
    return false;
  }

  const Mat4 world = scene.getWorldMatrix(target.entity_id);
  const Vec3 position = Vec3(world[3]);
  const Vec3 forward = glm::normalize(Vec3(-world[2]));
  const Vec3 look_target = position + forward;

  editor_camera.setLookAt(position, look_target);
  editor_camera.setVerticalFov(glm::radians(camera->vertical_fov_degrees));
  return true;
}

bool alignCameraToView(SceneInstance* scene, EditorCamera& editor_camera,
                       eastl::span<const EntityId> selected_ids) {
  if (scene == nullptr) {
    return false;
  }

  const AlignCameraTargetResult target =
      resolveAlignCameraTarget(*scene, selected_ids);
  if (!target.ok) {
    return false;
  }

  Entity* entity = scene->getEntity(target.entity_id);
  const CameraComponent* camera = scene->getCamera(target.entity_id);
  if (entity == nullptr || camera == nullptr) {
    return false;
  }

  const Vec3 before_position = entity->getPosition();
  const Quat before_rotation = entity->getRotation();
  const Vec3 before_scale = entity->getScale();
  const CameraComponent before_camera = *camera;

  const Vec3 world_position = editor_camera.getPosition();
  const Quat world_rotation = worldRotationFromCameraDirections(
      editor_camera.getForwardDirection(), editor_camera.getUpDirection());

  const Vec3 after_position =
      localPositionForWorldPosition(*scene, *entity, world_position);
  const Quat after_rotation =
      localRotationForWorldRotation(*scene, target.entity_id, world_rotation);
  const Vec3 after_scale = before_scale;

  CameraComponent after_camera = before_camera;
  after_camera.vertical_fov_degrees =
      glm::degrees(editor_camera.getVerticalFov());

  entity->setPosition(after_position);
  entity->setRotation(after_rotation);
  entity->setScale(after_scale);
  scene->setCamera(target.entity_id, after_camera);
  scene->markTransformsDirty();

  const SelectionSnapshot selection = currentSelectionSnapshot();
  pushDocumentCommand(makeAlignCameraToViewCommand(
      scene, target.entity_id, before_position, before_rotation, before_scale,
      before_camera, after_position, after_rotation, after_scale, after_camera,
      selection, selection));

  return true;
}

}  // namespace Blunder
