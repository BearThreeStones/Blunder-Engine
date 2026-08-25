#pragma once

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/scene/scene.h"

namespace Blunder {

inline Vec3 newSceneDirectionalLightPosition() { return Vec3(0.0f, 0.0f, 8.0f); }

inline Quat rotationBetweenUnitVectors(const Vec3& from, const Vec3& to) {
  const Vec3 f = glm::normalize(from);
  const Vec3 t = glm::normalize(to);
  const float d = glm::dot(f, t);
  if (d > 0.9999f) {
    return glm::identity<Quat>();
  }
  if (d < -0.9999f) {
    Vec3 axis = glm::cross(f, kWorldUp);
    if (glm::dot(axis, axis) < 1e-8f) {
      axis = glm::cross(f, kWorldRight);
    }
    return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
  }
  const Vec3 axis = glm::normalize(glm::cross(f, t));
  return glm::angleAxis(std::acos(std::clamp(d, -1.0f, 1.0f)), axis);
}

/// Same spirit as the old editor key `(0.45, 0.7, 0.55)` as N·L; emit is -L.
inline Quat newSceneDirectionalLightRotation() {
  const Vec3 to_light = glm::normalize(Vec3(0.45f, 0.7f, 0.55f));
  return rotationBetweenUnitVectors(Vec3(0.0f, 0.0f, -1.0f), -to_light);
}

inline void appendNewSceneStarterEntities(
    eastl::vector<SceneEntityDefinition>& entities) {
  SceneEntityDefinition camera{};
  camera.name = "Main Camera";
  camera.position = Vec3(0.0f, -8.0f, 2.0f);
  camera.has_camera = true;
  camera.camera.is_main = true;
  entities.push_back(eastl::move(camera));

  SceneEntityDefinition light{};
  light.name = "Directional Light";
  light.position = newSceneDirectionalLightPosition();
  light.rotation = newSceneDirectionalLightRotation();
  light.has_light = true;
  entities.push_back(eastl::move(light));
}

}  // namespace Blunder
