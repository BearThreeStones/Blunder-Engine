#include "runtime/function/ui/docking/dock_drag_controller.h"

#include <cmath>

namespace Blunder {

void DockDragController::begin(DockId dragged_widget_id, DockId source_container_id,
                               const glm::vec2& pointer, eastl::string title,
                               const DockRect& source_frame_rect, float title_bar_height,
                               float threshold_px) {
  if (m_state != DockDragState::idle) {
    return;
  }
  m_state = DockDragState::dragging;
  m_dragged_widget_id = dragged_widget_id;
  m_source_container_id = source_container_id;
  m_hovered_node_id = k_invalid_dock_id;
  m_hovered_slot = DockSlot::none;
  m_hovering_host_guide = false;
  m_hovered_host_slot = DockSlot::none;
  m_cross_container_id = k_invalid_dock_id;
  m_hovering_auto_hide = false;
  m_hovered_auto_hide_edge = DockEdge::left;
  m_auto_hide_tab_insert_index = k_invalid_auto_hide_tab_index;
  m_preview_rect = DockRect{};
  m_host_preview_rect = DockRect{};
  m_drag_preview_rect = DockRect{};
  m_drag_title = std::move(title);
  m_source_frame_rect = source_frame_rect;
  m_title_bar_height = title_bar_height;
  m_threshold_px = threshold_px;
  m_pointer = pointer;
  m_drag_start_pointer = pointer;
  m_preview_active = false;
  m_cancelled = false;
}

bool DockDragController::exceededDragThreshold() const {
  if (!isActive()) {
    return false;
  }
  const glm::vec2 delta = m_pointer - m_drag_start_pointer;
  const float distance_sq = delta.x * delta.x + delta.y * delta.y;
  return distance_sq >= m_threshold_px * m_threshold_px;
}

void DockDragController::updatePointer(const glm::vec2& pointer, bool drag_preview_enabled) {
  if (!isActive() || m_cancelled) {
    return;
  }
  m_pointer = pointer;
  if (drag_preview_enabled && !m_preview_active) {
    const glm::vec2 delta = pointer - m_drag_start_pointer;
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance >= m_threshold_px) {
      m_preview_active = true;
    }
  }
  if (m_preview_active) {
    updateDragPreviewRect(m_title_bar_height);
  }
}

void DockDragController::updateDragPreviewRect(float title_bar_height) {
  const float width =
      m_source_frame_rect.width > 0.0f ? m_source_frame_rect.width : 320.0f;
  const float height =
      m_source_frame_rect.height > 0.0f ? m_source_frame_rect.height : 220.0f;
  const float y_bias = title_bar_height * 0.5f;
  m_drag_preview_rect =
      makeDockRect(m_pointer.x - width * 0.5f, m_pointer.y - y_bias, width, height);
}

void DockDragController::setHover(DockId node_id, DockSlot slot,
                                  const DockRect& preview_rect) {
  if (!isActive() || m_cancelled) {
    return;
  }
  if (slot == DockSlot::none || node_id == k_invalid_dock_id) {
    clearHover();
    return;
  }
  clearAutoHideHover();
  clearHostHover();
  m_state = DockDragState::hovering_guide;
  m_hovered_node_id = node_id;
  m_hovered_slot = slot;
  m_cross_container_id = node_id;
  m_preview_rect = preview_rect;
}

void DockDragController::setCrossContainerEligible(DockId node_id) {
  if (!isActive() || m_cancelled) {
    return;
  }
  if (node_id == k_invalid_dock_id) {
    clearHover();
    return;
  }
  clearAutoHideHover();
  clearHostHover();
  m_state = DockDragState::hovering_guide;
  m_hovered_node_id = node_id;
  m_hovered_slot = DockSlot::none;
  m_cross_container_id = node_id;
  m_preview_rect = DockRect{};
}

void DockDragController::clearHover() {
  if (!isActive() || m_cancelled) {
    return;
  }
  m_hovered_node_id = k_invalid_dock_id;
  m_hovered_slot = DockSlot::none;
  m_cross_container_id = k_invalid_dock_id;
  m_preview_rect = DockRect{};
  if (!m_hovering_auto_hide && !m_hovering_host_guide) {
    m_state = DockDragState::dragging;
  }
}

void DockDragController::setHostHover(DockSlot slot, const DockRect& preview_rect) {
  if (!isActive() || m_cancelled || slot == DockSlot::none) {
    return;
  }
  clearHover();
  clearAutoHideHover();
  m_hovering_host_guide = true;
  m_hovered_host_slot = slot;
  m_host_preview_rect = preview_rect;
  m_state = DockDragState::hovering_guide;
}

void DockDragController::clearHostHover() {
  if (!isActive() || m_cancelled) {
    return;
  }
  m_hovering_host_guide = false;
  m_hovered_host_slot = DockSlot::none;
  m_host_preview_rect = DockRect{};
  if (!m_hovering_auto_hide && m_hovered_slot == DockSlot::none &&
      m_cross_container_id == k_invalid_dock_id) {
    m_state = DockDragState::dragging;
  }
}

void DockDragController::setAutoHideHover(DockEdge edge, int tab_insert_index) {
  if (!isActive() || m_cancelled) {
    return;
  }
  clearHover();
  clearHostHover();
  m_hovering_auto_hide = true;
  m_hovered_auto_hide_edge = edge;
  m_auto_hide_tab_insert_index = tab_insert_index;
  m_state = DockDragState::hovering_guide;
}

void DockDragController::clearAutoHideHover() {
  if (!isActive() || m_cancelled) {
    return;
  }
  m_hovering_auto_hide = false;
  m_auto_hide_tab_insert_index = k_invalid_auto_hide_tab_index;
  if (m_hovered_slot == DockSlot::none && m_cross_container_id == k_invalid_dock_id &&
      !m_hovering_host_guide) {
    m_state = DockDragState::dragging;
  }
}

bool DockDragController::markDropped() {
  if (m_hovering_auto_hide || m_hovering_host_guide || m_state != DockDragState::hovering_guide ||
      m_cancelled || m_hovered_slot == DockSlot::none) {
    return false;
  }
  m_state = DockDragState::dropped;
  return true;
}

bool DockDragController::markHostDockDropped() {
  if (!m_hovering_host_guide || m_cancelled) {
    return false;
  }
  m_state = DockDragState::dropped;
  return true;
}

bool DockDragController::markAutoHideDropped() {
  if (!m_hovering_auto_hide || m_cancelled) {
    return false;
  }
  m_state = DockDragState::dropped;
  return true;
}

void DockDragController::cancel() {
  if (!isActive()) {
    return;
  }
  m_cancelled = true;
  m_preview_active = false;
}

void DockDragController::reset() {
  m_state = DockDragState::idle;
  m_dragged_widget_id = k_invalid_dock_id;
  m_source_container_id = k_invalid_dock_id;
  m_hovered_node_id = k_invalid_dock_id;
  m_hovered_slot = DockSlot::none;
  m_hovering_host_guide = false;
  m_hovered_host_slot = DockSlot::none;
  m_cross_container_id = k_invalid_dock_id;
  m_hovering_auto_hide = false;
  m_hovered_auto_hide_edge = DockEdge::left;
  m_auto_hide_tab_insert_index = k_invalid_auto_hide_tab_index;
  m_preview_rect = DockRect{};
  m_host_preview_rect = DockRect{};
  m_drag_preview_rect = DockRect{};
  m_source_frame_rect = DockRect{};
  m_drag_title.clear();
  m_pointer = glm::vec2{0.0f, 0.0f};
  m_drag_start_pointer = glm::vec2{0.0f, 0.0f};
  m_title_bar_height = 26.0f;
  m_threshold_px = k_default_drag_threshold;
  m_preview_active = false;
  m_cancelled = false;
}

}  // namespace Blunder
