#pragma once

namespace Blunder {

/// Geometry shared by navigate_gizmo.slang and CPU hit testing.
struct NavigateGizmoMetrics {
  static constexpr float kBgRadius = 1.0f;
  /// Blender: arm ends at (1 - AXIS_HANDLE_SIZE) = 0.80.
  static constexpr float kArmLength = 0.80f;
  /// Blender AXIS_HANDLE_SIZE = 0.20 in normalized widget space.
  static constexpr float kBallRadius = 0.20f;
  static constexpr float kNegBallRadius = 0.20f;
  static constexpr float kCenterRadius = 0.0f;
  /// ~(gizmo_size/40) / gizmo_size in normalized widget space.
  static constexpr float kArmHalfWidth = 0.025f;
  /// Screen-space SDF stroke width (px); match TransformGizmoMetrics::k_sdf_stroke_width_px.
  static constexpr float kNegBallBorderWidthPx = 0.3f;
  /// Fixed neutral gray for muted axis fill (not view background).
  static constexpr float kNegBallFillGray = 0.20f;
  /// lerp(fillGray, axisColor, t) for negative-ball interior.
  static constexpr float kNegBallFillMix = 0.25f;
  static constexpr float kHitSlop = 1.15f;
};
}  // namespace Blunder
