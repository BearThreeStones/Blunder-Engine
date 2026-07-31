#pragma once

#include <algorithm>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/render/forward/forward_frame_state.h"
#include "runtime/function/render/mesh_preview/mesh_preview_framing.h"
#include "runtime/function/render/mesh_preview/mesh_preview_studio_lights.h"

namespace Blunder {

inline BlinnPhongEditorSettings meshPreviewStudioLightsToShading(
    const MeshPreviewStudioLights& lights) {
  BlinnPhongEditorSettings shading{};
  const float key_dir_len = glm::length(lights.key_light_direction);
  shading.light_direction =
      key_dir_len > 1e-4f ? lights.key_light_direction / key_dir_len
                          : glm::vec3(0.45f, 0.7f, 0.55f);
  shading.light_color = lights.key_light_color;
  shading.ambient_color =
      lights.ambient_color + lights.fill_light_color * 0.35f;
  shading.diffuse_color = glm::vec3(1.0f);
  shading.specular_color = glm::vec3(0.4f);
  shading.shininess = 32.0f;
  shading.unlit = false;
  return shading;
}

inline ForwardFrameState buildMeshPreviewForwardFrameState(
    const MeshPreviewCameraFrame& framing,
    const MeshPreviewStudioLights& lights, uint32_t width, uint32_t height) {
  ForwardFrameState frame_state{};
  frame_state.view = glm::lookAt(framing.eye, framing.target, framing.up);
  const float aspect =
      height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
  const float distance =
      std::max(glm::length(framing.eye - framing.target), 0.1f);
  frame_state.near_clip = 0.1f;
  frame_state.far_clip = std::max(distance * 10.0f, 100.0f);
  frame_state.projection =
      glm::perspectiveZO(framing.vertical_fov_rad, aspect, frame_state.near_clip,
                         frame_state.far_clip);
  frame_state.projection[1][1] *= -1.0f;
  frame_state.camera_position = framing.eye;
  frame_state.camera_forward =
      glm::normalize(framing.target - framing.eye);
  frame_state.vertical_fov = framing.vertical_fov_rad;
  frame_state.shadows_enabled = lights.shadows_enabled;
  frame_state.viewport_width = std::max(1u, width);
  frame_state.viewport_height = std::max(1u, height);
  frame_state.shading = meshPreviewStudioLightsToShading(lights);
  return frame_state;
}

}  // namespace Blunder
