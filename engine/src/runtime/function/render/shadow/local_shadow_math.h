#pragma once

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "runtime/core/math/coordinate_system.h"
#include "runtime/function/render/shadow/virtual_shadow_map.h"

namespace Blunder {

inline glm::mat4 makePerspectiveShadowProjection(float fov_y_radians, float near_plane,
                                                 float far_plane) {
  glm::mat4 proj = glm::perspectiveZO(fov_y_radians, 1.0f, near_plane, far_plane);
  proj[1][1] *= -1.0f;
  return proj;
}

inline void cubeFaceDirectionUp(uint32_t face, glm::vec3& out_dir, glm::vec3& out_up) {
  switch (face) {
    case 0:
      out_dir = glm::vec3(1.0f, 0.0f, 0.0f);
      out_up = glm::vec3(0.0f, 0.0f, -1.0f);
      break;
    case 1:
      out_dir = glm::vec3(-1.0f, 0.0f, 0.0f);
      out_up = glm::vec3(0.0f, 0.0f, -1.0f);
      break;
    case 2:
      out_dir = glm::vec3(0.0f, 1.0f, 0.0f);
      out_up = glm::vec3(0.0f, 0.0f, 1.0f);
      break;
    case 3:
      out_dir = glm::vec3(0.0f, -1.0f, 0.0f);
      out_up = glm::vec3(0.0f, 0.0f, -1.0f);
      break;
    case 4:
      out_dir = glm::vec3(0.0f, 0.0f, 1.0f);
      out_up = glm::vec3(0.0f, -1.0f, 0.0f);
      break;
    default:
      out_dir = glm::vec3(0.0f, 0.0f, -1.0f);
      out_up = glm::vec3(0.0f, -1.0f, 0.0f);
      break;
  }
}

inline glm::mat4 makePointCubeFaceViewProjection(const glm::vec3& light_pos,
                                                 uint32_t face, float range) {
  glm::vec3 dir(0.0f);
  glm::vec3 up(0.0f, 0.0f, 1.0f);
  cubeFaceDirectionUp(face, dir, up);
  const glm::mat4 view = glm::lookAt(light_pos, light_pos + dir, up);
  const glm::mat4 proj = makePerspectiveShadowProjection(
      glm::radians(90.0f), k_point_shadow_near, std::max(range, k_point_shadow_near + 0.01f));
  return proj * view;
}

inline glm::mat4 makeSpotViewProjection(const glm::vec3& light_pos,
                                        const glm::vec3& emit, float outer_degrees,
                                        float range) {
  glm::vec3 z = glm::length(emit) > 1e-4f ? glm::normalize(emit)
                                          : glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 up = kWorldUp;
  if (std::abs(glm::dot(z, up)) > 0.95f) {
    up = kWorldForward;
  }
  const glm::mat4 view = glm::lookAt(light_pos, light_pos + z, up);
  const float fov = glm::radians(std::max(outer_degrees * 2.0f, 1.0f));
  const glm::mat4 proj = makePerspectiveShadowProjection(
      fov, k_point_shadow_near, std::max(range, k_point_shadow_near + 0.01f));
  return proj * view;
}

}  // namespace Blunder
