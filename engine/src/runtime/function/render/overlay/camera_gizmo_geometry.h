#pragma once

#include <cmath>

#include "runtime/core/math/math_types.h"

namespace Blunder {

constexpr float kCameraGizmoDisplayDistance = 1.0f;

struct CameraGizmoFrame {
  Vec3 origin;
  Vec3 corners[4]; // TL, TR, BR, BL in local space
  Vec3 up_triangle[3];
};

// Local-space frame with look along -Z (glTF camera convention), up +Y.
// Frame lies in the XY plane at z = -display_distance.
inline CameraGizmoFrame buildCameraGizmoFrameLocal(float vertical_fov_radians,
                                                   float aspect,
                                                   float display_distance) {
  constexpr float kUpTriangleHeightFactor = 0.25f;

  CameraGizmoFrame frame{};
  frame.origin = Vec3(0.0f);

  const float half_h = display_distance * std::tan(vertical_fov_radians * 0.5f);
  const float half_w = half_h * aspect;
  const float z = -display_distance;

  frame.corners[0] = Vec3(-half_w, half_h, z);   // TL
  frame.corners[1] = Vec3(half_w, half_h, z);    // TR
  frame.corners[2] = Vec3(half_w, -half_h, z);   // BR
  frame.corners[3] = Vec3(-half_w, -half_h, z);  // BL

  const float tip_y = half_h + half_h * kUpTriangleHeightFactor;
  frame.up_triangle[0] = frame.corners[0];
  frame.up_triangle[1] = Vec3(0.0f, tip_y, z);
  frame.up_triangle[2] = frame.corners[1];

  return frame;
}

}  // namespace Blunder
