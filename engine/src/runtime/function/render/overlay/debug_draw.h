#pragma once

#include <cstdint>

#include "runtime/core/math/math_types.h"
#include "runtime/function/render/overlay/debug_draw_geometry.h"
#include "runtime/function/render/overlay/gizmo_line_command.h"

namespace Blunder {

struct OverlayState;

/// Immediate-mode debug lines. World units, Z-up. Not authorship gizmos.
class DebugDraw final {
 public:
  static constexpr float k_default_width_px = 1.0f;
  static constexpr float k_xray_alpha = 0.22f;

  static void line(const Vec3& a, const Vec3& b, const Vec4& color,
                   float duration_s = 0.0f,
                   float width_px = k_default_width_px);
  static void ray(const Vec3& origin, const Vec3& direction, const Vec4& color,
                  float duration_s = 0.0f,
                  float width_px = k_default_width_px);
  static void arrow(const Vec3& from, const Vec3& to, const Vec4& color,
                    float duration_s = 0.0f,
                    float width_px = k_default_width_px);
  static void wireBox(const Vec3& center, const Vec3& size, const Vec4& color,
                      float duration_s = 0.0f,
                      float width_px = k_default_width_px);
  static void wireSphere(const Vec3& center, float radius, const Vec4& color,
                         float duration_s = 0.0f,
                         float width_px = k_default_width_px);
  static void wireCapsule(const Vec3& a, const Vec3& b, float radius,
                          const Vec4& color, float duration_s = 0.0f,
                          float width_px = k_default_width_px);
  static void cross(const Vec3& point, float size, const Vec4& color,
                    float duration_s = 0.0f,
                    float width_px = k_default_width_px);

  /// Drop this-frame commands and tick persist. Call after the GPU record
  /// that expanded the current buffer.
  static void endFrame(float delta_time);

  static void reset();
  static uint32_t commandCount();

  /// Player host only. Editor viewports always draw.
  static void setInGameEnabled(bool enabled);
  static bool inGameEnabled();

  static void expandLines(GizmoLineCommandStream& out,
                          const OverlayState& state);
};

}  // namespace Blunder
