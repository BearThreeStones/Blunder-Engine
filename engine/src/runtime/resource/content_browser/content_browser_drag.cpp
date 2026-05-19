#include "runtime/resource/content_browser/content_browser_drag.h"

#include <cmath>

namespace Blunder {

void ContentBrowserDragController::reset() {
  m_source_path.clear();
  m_press_x = 0.0f;
  m_press_y = 0.0f;
  m_press_active = false;
  m_is_dragging = false;
}

void ContentBrowserDragController::beginPress(const eastl::string& virtual_path,
                                              float x, float y) {
  m_source_path = virtual_path;
  m_press_x = x;
  m_press_y = y;
  m_press_active = true;
  m_is_dragging = false;
}

void ContentBrowserDragController::updateMove(float x, float y) {
  if (!m_press_active || m_is_dragging) {
    return;
  }
  const float dx = x - m_press_x;
  const float dy = y - m_press_y;
  const float distance_sq = dx * dx + dy * dy;
  if (distance_sq >= k_drag_threshold_px * k_drag_threshold_px) {
    m_is_dragging = true;
  }
}

void ContentBrowserDragController::endPress() {
  m_press_active = false;
  if (!m_is_dragging) {
    m_source_path.clear();
  }
}

}  // namespace Blunder
