#include "runtime/function/render/gpu_driven/gpu_driven_cull.h"

#include <cmath>

#include <glm/geometric.hpp>

namespace Blunder {

void extractFrustumPlanes(const glm::mat4& view_projection, glm::vec4 out_planes[6]) {
  const glm::mat4& m = view_projection;
  out_planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0],
                            m[3][3] + m[3][0]);
  out_planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0],
                            m[3][3] - m[3][0]);
  out_planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1],
                            m[3][3] + m[3][1]);
  out_planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1],
                            m[3][3] - m[3][1]);
  out_planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2],
                            m[3][3] + m[3][2]);
  out_planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2],
                            m[3][3] - m[3][2]);
  for (int i = 0; i < 6; ++i) {
    const float len = glm::length(glm::vec3(out_planes[i]));
    if (len > 1e-8f) {
      out_planes[i] /= len;
    }
  }
}

bool sphereInsideFrustum(const glm::vec3& center, float radius,
                         const glm::vec4 planes[6]) {
  for (int i = 0; i < 6; ++i) {
    const float distance =
        glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
    if (distance < -radius) {
      return false;
    }
  }
  return true;
}

bool meshletConeCulled(const glm::vec3& world_center, const glm::vec3& world_axis,
                       float cone_cutoff, float radius, const glm::vec3& camera) {
  if (cone_cutoff <= 0.0f) {
    return false;
  }
  const float axis_len_sq = glm::dot(world_axis, world_axis);
  if (axis_len_sq < 1e-12f) {
    return false;
  }
  const glm::vec3 from_camera = world_center - camera;
  const float dist = std::sqrt(glm::dot(from_camera, from_camera));
  if (dist <= radius + 1e-3f) {
    return false;
  }
  const glm::vec3 axis = world_axis * (1.0f / std::sqrt(axis_len_sq));
  return glm::dot(from_camera, axis) >= cone_cutoff * dist + radius;
}

bool meshletHiZOccluded(const glm::vec4& clip_center, float radius_ndc,
                        float sampled_min_depth) {
  if (clip_center.w <= 1e-6f) {
    return false;
  }
  const float ndc_z = clip_center.z / clip_center.w;
  const float nearest = ndc_z - radius_ndc;
  return nearest > sampled_min_depth;
}

}  // namespace Blunder
