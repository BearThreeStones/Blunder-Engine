#include "runtime/project/play_pose_preview.h"

#include "EASTL/vector.h"

#include "runtime/function/scene/entity.h"
#include "runtime/function/scene/scene_instance.h"
#include "runtime/function/scene/scene_system.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Blunder {
namespace {

Mat4 localMatrixFromPose(const PlayPoseLocalTrs& pose) {
  const Vec3 position(pose.t[0], pose.t[1], pose.t[2]);
  const Quat rotation(pose.r[3], pose.r[0], pose.r[1], pose.r[2]);
  const Vec3 scale(pose.s[0], pose.s[1], pose.s[2]);
  const Mat4 translation = glm::translate(Mat4(1.0f), position);
  const Mat4 rotation_m = glm::mat4_cast(rotation);
  const Mat4 scale_m = glm::scale(Mat4(1.0f), scale);
  return translation * rotation_m * scale_m;
}

Mat4 localMatrixForOverlay(const Entity& entity,
                           const PlayPoseOverlayMap* overlay) {
  if (overlay != nullptr && !entity.getName().empty()) {
    const auto it = overlay->find(entity.getName());
    if (it != overlay->end()) {
      return localMatrixFromPose(it->second);
    }
  }
  return entity.getLocalMatrix();
}

}  // namespace

void collectPlayPoses(const SceneInstance& scene, PlayIpcPosesRecord& out) {
  out.entities.clear();
  scene.forEachEntity([&](EntityId id, const Entity& entity) {
    (void)id;
    if (entity.getName().empty() || entity.isTombstoned()) {
      return;
    }
    PlayIpcPoseEntity pose;
    pose.name = entity.getName().c_str();
    const Vec3& t = entity.getPosition();
    pose.t[0] = t.x;
    pose.t[1] = t.y;
    pose.t[2] = t.z;
    const Quat& r = entity.getRotation();
    pose.r[0] = r.x;
    pose.r[1] = r.y;
    pose.r[2] = r.z;
    pose.r[3] = r.w;
    const Vec3& s = entity.getScale();
    pose.s[0] = s.x;
    pose.s[1] = s.y;
    pose.s[2] = s.z;
    out.entities.push_back(pose);
  });
}

void collectPlayPosesFromActiveScene(const SceneSystem* scenes,
                                     PlayIpcPosesRecord& out) {
  out.entities.clear();
  if (scenes == nullptr) {
    return;
  }
  const SceneInstance* instance = scenes->getActiveInstance();
  if (instance == nullptr) {
    return;
  }
  collectPlayPoses(*instance, out);
}

Mat4 worldMatrixWithPlayPoseOverlay(const SceneInstance& scene, EntityId id,
                                    const PlayPoseOverlayMap* overlay) {
  if (overlay == nullptr || overlay->empty()) {
    return scene.getWorldMatrix(id);
  }
  if (!isValid(id) || scene.getEntity(id) == nullptr) {
    return Mat4(1.0f);
  }

  eastl::vector<EntityId> chain;
  EntityId cursor = id;
  while (isValid(cursor)) {
    chain.push_back(cursor);
    const Entity* entity = scene.getEntity(cursor);
    if (entity == nullptr) {
      break;
    }
    cursor = entity->getParentId();
  }

  Mat4 world = scene.getSceneToWorldMatrix();
  for (size_t i = chain.size(); i > 0; --i) {
    const Entity* entity = scene.getEntity(chain[i - 1]);
    if (entity == nullptr) {
      continue;
    }
    if (!entity->isEnabled()) {
      continue;
    }
    world = world * localMatrixForOverlay(*entity, overlay);
  }
  return world;
}

}  // namespace Blunder
