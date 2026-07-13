#pragma once

#include <algorithm>
#include <cstdint>

#include "runtime/function/render/overlay/navigate_gizmo_style.h"

namespace Blunder {

struct Rect2f {
  float x{0.0f};
  float y{0.0f};
  float width{0.0f};
  float height{0.0f};
};

inline bool rectContains(const Rect2f& rect, float x, float y) {
  return x >= rect.x && x < rect.x + rect.width && y >= rect.y &&
         y < rect.y + rect.height;
}

struct NavigateGizmoLayout {
  Rect2f gizmo_rect{};
  Rect2f projection_button{};
  float gizmo_size{0.0f};
  bool visible{false};
};

constexpr float kNavigateGizmoBaseSize = kBlenderNavigateGizmoDiameterPx;
constexpr float kNavigateGizmoMargin = 10.0f;
constexpr float kNavigateGizmoMinSize = 30.0f;
constexpr float kProjectionButtonWidth = 56.0f;
constexpr float kProjectionButtonHeight = 16.0f;
constexpr float kProjectionButtonGap = 2.0f;
constexpr float kProjectionButtonHitSlop = 16.0f;

inline NavigateGizmoLayout computeNavigateGizmoLayout(uint32_t viewport_width,
                                                      uint32_t viewport_height) {
  NavigateGizmoLayout layout{};
  const float vw = static_cast<float>(viewport_width);
  const float vh = static_cast<float>(viewport_height);
  float gizmo_size = kNavigateGizmoBaseSize;
  const float max_size = std::min(vw, vh) * 0.35f;
  gizmo_size = std::min(gizmo_size, max_size);
  if (vw < 80.0f || vh < 48.0f) {
    return layout;
  }
  // Keep hit target / Slint layout stable when the dock squishes the viewport during resize.
  gizmo_size = std::max(gizmo_size, kNavigateGizmoMinSize);

  layout.visible = true;
  layout.gizmo_size = gizmo_size;
  layout.gizmo_rect = {vw - gizmo_size - kNavigateGizmoMargin,
                       kNavigateGizmoMargin, gizmo_size, gizmo_size};

  const float button_x =
      layout.gizmo_rect.x + (gizmo_size - kProjectionButtonWidth) * 0.5f;
  const float button_y =
      layout.gizmo_rect.y + gizmo_size + kProjectionButtonGap;
  layout.projection_button = {button_x, button_y, kProjectionButtonWidth,
                              kProjectionButtonHeight};
  return layout;
}

/// Slint ViewportProjectionToggle TouchArea bounds (viewport_projection_toggle.slint).
inline bool hitSlintProjectionToggleChrome(float window_x, float window_y,
                                         float viewport_origin_x,
                                         float viewport_origin_y,
                                         float viewport_width,
                                         float viewport_height) {
  if (viewport_width < 80.0f || viewport_height < 48.0f) {
    return false;
  }
  const float gizmo_size =
      std::max(kNavigateGizmoMinSize,
               std::min(kNavigateGizmoBaseSize,
                        std::min(viewport_width, viewport_height) * 0.35f));
  const float chrome_x =
      viewport_origin_x + viewport_width - gizmo_size - kNavigateGizmoMargin;
  const float chrome_y = viewport_origin_y + kNavigateGizmoMargin;
  const float chrome_w = gizmo_size + kNavigateGizmoMargin;
  const float chrome_h = gizmo_size + 26.0f;
  return window_x >= chrome_x && window_x < chrome_x + chrome_w &&
         window_y >= chrome_y && window_y < chrome_y + chrome_h;
}

/// Hit-test Persp/Iso in window logical coords (matches viewport_projection_toggle.slint).
inline bool hitProjectionToggleAtWindowLogical(float window_x, float window_y,
                                               float viewport_origin_x,
                                               float viewport_origin_y,
                                               float viewport_width,
                                               float viewport_height) {
  if (hitSlintProjectionToggleChrome(window_x, window_y, viewport_origin_x,
                                     viewport_origin_y, viewport_width,
                                     viewport_height)) {
    return true;
  }
  if (viewport_width < 80.0f || viewport_height < 48.0f) {
    return false;
  }
  const float gizmo_size =
      std::max(kNavigateGizmoMinSize,
               std::min(kNavigateGizmoBaseSize,
                        std::min(viewport_width, viewport_height) * 0.35f));
  const float btn_x = viewport_origin_x + viewport_width - gizmo_size -
                      kNavigateGizmoMargin +
                      (gizmo_size - kProjectionButtonWidth) * 0.5f;
  const float btn_y =
      viewport_origin_y + kNavigateGizmoMargin + gizmo_size + kProjectionButtonGap;
  const float slop = kProjectionButtonHitSlop;
  if (window_x >= btn_x - slop && window_x < btn_x + kProjectionButtonWidth + slop &&
      window_y >= btn_y - slop &&
      window_y < btn_y + kProjectionButtonHeight + slop) {
    return true;
  }
  return false;
}

/// Hit-test the navigate gizmo + Persp/Iso label area in viewport-local logical coords.
/// Matches the Slint ViewportProjectionToggle TouchArea bounds exactly.
inline bool hitViewportTopRightChromeLocal(float viewport_local_x,
                                           float viewport_local_y,
                                           float viewport_width,
                                           float viewport_height) {
  if (viewport_width < 80.0f || viewport_height < 48.0f) {
    return false;
  }
  const float gizmo_size =
      std::max(kNavigateGizmoMinSize,
               std::min(kNavigateGizmoBaseSize,
                        std::min(viewport_width, viewport_height) * 0.35f));
  // Slint TouchArea: x = vp_w - gizmo_size - 10, y = 10,
  //                   w = gizmo_size + 10,        h = gizmo_size + 26
  const float chrome_x = viewport_width - gizmo_size - kNavigateGizmoMargin;
  const float chrome_y = kNavigateGizmoMargin;
  const float chrome_w = gizmo_size + kNavigateGizmoMargin;
  const float chrome_h = gizmo_size + 26.0f;
  return viewport_local_x >= chrome_x && viewport_local_x < chrome_x + chrome_w &&
         viewport_local_y >= chrome_y && viewport_local_y < chrome_y + chrome_h;
}

/// Hit-test Persp/Iso button in viewport-local logical coordinates (matches Slint layout).
inline bool hitTestProjectionButtonViewportLocal(float viewport_local_x,
                                                 float viewport_local_y,
                                                 uint32_t viewport_width,
                                                 uint32_t viewport_height) {
  if (viewport_width < 80u || viewport_height < 48u) {
    return false;
  }
  const NavigateGizmoLayout layout =
      computeNavigateGizmoLayout(viewport_width, viewport_height);
  if (!layout.visible) {
    return false;
  }
  // Match the visible label; extend to full gizmo width for easier clicking.
  Rect2f hit = layout.projection_button;
  hit.x = layout.gizmo_rect.x;
  hit.width = layout.gizmo_rect.width;
  hit.y -= kProjectionButtonHitSlop;
  hit.height += kProjectionButtonHitSlop * 2.0f;
  if (rectContains(hit, viewport_local_x, viewport_local_y)) {
    return true;
  }
  // Fallback band under the nav gizmo (covers DPI / layout drift).
  const float vw = static_cast<float>(viewport_width);
  const float vh = static_cast<float>(viewport_height);
  return viewport_local_x >= vw - 150.0f && viewport_local_y >= 35.0f &&
         viewport_local_y <= 125.0f;
}

}  // namespace Blunder
