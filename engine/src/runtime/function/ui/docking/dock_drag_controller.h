#pragma once

#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

class DockDragController final {
 public:
  DockDragState state() const { return m_state; }
  bool isActive() const { return m_state != DockDragState::idle; }
  bool isHoveringGuide() const { return m_state == DockDragState::hovering_guide; }

  DockId draggedWidgetId() const { return m_dragged_widget_id; }
  DockId sourceContainerId() const { return m_source_container_id; }
  DockId hoveredNodeId() const { return m_hovered_node_id; }
  DockSlot hoveredSlot() const { return m_hovered_slot; }
  const DockRect& previewRect() const { return m_preview_rect; }
  const glm::vec2& pointer() const { return m_pointer; }

  void begin(DockId dragged_widget_id, DockId source_container_id, const glm::vec2& pointer);
  void updatePointer(const glm::vec2& pointer);
  void setHover(DockId node_id, DockSlot slot, const DockRect& preview_rect);
  void clearHover();
  bool markDropped();
  void reset();

 private:
  DockDragState m_state{DockDragState::idle};
  DockId m_dragged_widget_id{k_invalid_dock_id};
  DockId m_source_container_id{k_invalid_dock_id};
  DockId m_hovered_node_id{k_invalid_dock_id};
  DockSlot m_hovered_slot{DockSlot::none};
  DockRect m_preview_rect{};
  glm::vec2 m_pointer{0.0f, 0.0f};
};

}
