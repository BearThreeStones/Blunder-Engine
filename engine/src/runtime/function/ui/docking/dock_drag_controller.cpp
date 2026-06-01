#include "runtime/function/ui/docking/dock_drag_controller.h"

namespace Blunder {

void DockDragController::begin(DockId dragged_widget_id, DockId source_container_id,
                               const glm::vec2& pointer) {
  if (m_state != DockDragState::idle) {
    return;
  }
  m_state = DockDragState::dragging;
  m_dragged_widget_id = dragged_widget_id;
  m_source_container_id = source_container_id;
  m_hovered_node_id = k_invalid_dock_id;
  m_hovered_slot = DockSlot::none;
  m_preview_rect = DockRect{};
  m_pointer = pointer;
}

void DockDragController::updatePointer(const glm::vec2& pointer) {
  if (!isActive()) {
    return;
  }
  m_pointer = pointer;
}

void DockDragController::setHover(DockId node_id, DockSlot slot,
                                  const DockRect& preview_rect) {
  if (!isActive()) {
    return;
  }
  if (slot == DockSlot::none || node_id == k_invalid_dock_id) {
    clearHover();
    return;
  }
  m_state = DockDragState::hovering_guide;
  m_hovered_node_id = node_id;
  m_hovered_slot = slot;
  m_preview_rect = preview_rect;
}

void DockDragController::clearHover() {
  if (!isActive()) {
    return;
  }
  m_state = DockDragState::dragging;
  m_hovered_node_id = k_invalid_dock_id;
  m_hovered_slot = DockSlot::none;
  m_preview_rect = DockRect{};
}

bool DockDragController::markDropped() {
  if (m_state != DockDragState::hovering_guide) {
    return false;
  }
  m_state = DockDragState::dropped;
  return true;
}

void DockDragController::reset() {
  m_state = DockDragState::idle;
  m_dragged_widget_id = k_invalid_dock_id;
  m_source_container_id = k_invalid_dock_id;
  m_hovered_node_id = k_invalid_dock_id;
  m_hovered_slot = DockSlot::none;
  m_preview_rect = DockRect{};
  m_pointer = glm::vec2{0.0f, 0.0f};
}

}
