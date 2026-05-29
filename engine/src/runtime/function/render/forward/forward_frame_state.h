#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "runtime/function/render/blinn_phong_editor_settings.h"
#include "runtime/function/render/editor_camera.h"

namespace Blunder {

enum class ForwardGridPlane : uint32_t {
  xy = 0,
  xz = 1,
  yz = 2,
};

/// Per-frame camera and editor shading inputs for the forward pass.
struct ForwardFrameState {
  glm::mat4 view{1.0f};
  glm::mat4 projection{1.0f};
  glm::vec3 camera_position{0.0f};
  glm::vec3 camera_forward{0.0f, 0.0f, -1.0f};
  float camera_distance{6.0f};
  float near_clip{0.1f};
  float far_clip{1000.0f};
  float vertical_fov{0.785398f};
  float ortho_size{10.0f};
  EditorCamera::ProjectionMode projection_mode{
      EditorCamera::ProjectionMode::perspective};
  float projection_transition_t{0.0f};
  ForwardGridPlane grid_plane{ForwardGridPlane::xy};
  BlinnPhongEditorSettings shading;
  glm::mat4 light_view{1.0f};
  glm::mat4 light_projection{1.0f};
  glm::mat4 light_view_projection{1.0f};
  float shadow_bias{0.002f};
  bool shadows_enabled{true};
  uint32_t viewport_width{1};
  uint32_t viewport_height{1};
};

}  // namespace Blunder
