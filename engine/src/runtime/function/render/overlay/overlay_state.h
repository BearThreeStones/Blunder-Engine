#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "runtime/function/render/editor_camera.h"
#include "runtime/function/render/forward/forward_frame_state.h"

namespace Blunder {

/// Per-frame context snapshot for overlay rendering.
struct OverlayState {
  glm::mat4 view{1.0f};
  glm::mat4 projection{1.0f};
  glm::vec3 camera_position{0.0f};
  glm::vec3 camera_forward{0.0f, 0.0f, -1.0f};
  float camera_distance{6.0f};
  float near_clip{0.1f};
  float far_clip{1000.0f};
  float vertical_fov{0.785398f};
  float ortho_size{10.0f};
  bool is_perspective{true};
  ForwardGridPlane grid_plane{ForwardGridPlane::xy};
  uint32_t viewport_width{1};
  uint32_t viewport_height{1};
  uint32_t frame_index{0};

  /// Populate from a ForwardFrameState.
  static OverlayState fromFrameState(const ForwardFrameState& fs,
                                     uint32_t current_frame) {
    OverlayState s;
    s.view = fs.view;
    s.projection = fs.projection;
    s.camera_position = fs.camera_position;
    s.camera_forward = fs.camera_forward;
    s.camera_distance = fs.camera_distance;
    s.near_clip = fs.near_clip;
    s.far_clip = fs.far_clip;
    s.vertical_fov = fs.vertical_fov;
    s.ortho_size = fs.ortho_size;
    s.is_perspective =
        (fs.projection_mode != EditorCamera::ProjectionMode::orthographic);
    s.grid_plane = fs.grid_plane;
    s.viewport_width = fs.viewport_width;
    s.viewport_height = fs.viewport_height;
    s.frame_index = current_frame;
    return s;
  }
};

}  // namespace Blunder
