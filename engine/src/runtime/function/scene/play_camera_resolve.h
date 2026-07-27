#pragma once

#include <cstddef>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/core/math/math_types.h"
#include "runtime/function/scene/camera_component.h"
#include "runtime/function/scene/entity_id.h"

namespace Blunder {

struct PlayCameraResolveInput {
  // One candidate per entity that has a CameraComponent.
  EntityId entity_id{k_invalid_entity_id};
  Mat4 world{1.0f};
  CameraComponent camera{};
};

struct ResolvedPlayCamera {
  bool ok{false};
  EntityId entity_id{k_invalid_entity_id};
  Mat4 view{1.0f};
  Mat4 projection{1.0f};
  Vec3 position{0.0f};
  Vec3 forward{0.0f, 1.0f, 0.0f};
  float near_clip{0.1f};
  float far_clip{1000.0f};
  float vertical_fov_radians{glm::radians(45.0f)};
};

/// Prefer is_main; else first entry in array order. Empty → ok=false.
inline ResolvedPlayCamera resolvePlayCamera(const PlayCameraResolveInput* cameras,
                                            size_t count, float aspect) {
  ResolvedPlayCamera result{};
  if (cameras == nullptr || count == 0) {
    return result;
  }

  const PlayCameraResolveInput* chosen = &cameras[0];
  for (size_t i = 0; i < count; ++i) {
    if (cameras[i].camera.is_main) {
      chosen = &cameras[i];
      break;
    }
  }

  const CameraComponent& camera = chosen->camera;
  const Mat4& world = chosen->world;

  const Vec3 position = Vec3(world[3]);
  const Vec3 forward = glm::normalize(Vec3(-world[2]));
  const Vec3 up = kWorldUp;
  const float vertical_fov_radians = glm::radians(camera.vertical_fov_degrees);

  result.ok = true;
  result.entity_id = chosen->entity_id;
  result.position = position;
  result.forward = forward;
  result.view = glm::lookAt(position, position + forward, up);
  result.projection =
      glm::perspective(vertical_fov_radians, aspect, camera.near_clip, camera.far_clip);
  result.projection[1][1] *= -1.0f;
  result.near_clip = camera.near_clip;
  result.far_clip = camera.far_clip;
  result.vertical_fov_radians = vertical_fov_radians;
  return result;
}

}  // namespace Blunder
