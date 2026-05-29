#pragma once

namespace Blunder {

/// Geometry shared by navigate_gizmo.slang and CPU hit testing.
struct NavigateGizmoMetrics {
  static constexpr float kArmLength = 0.68f;
  static constexpr float kBallRadius = 0.24f;
  static constexpr float kNegBallRadius = 0.12f;
  static constexpr float kCenterRadius = 0.22f;
  static constexpr float kArmHalfWidth = 0.04f;
  static constexpr float kBgRadius = 1.0f;
  static constexpr float kHitSlop = 1.15f;
};

}  // namespace Blunder
