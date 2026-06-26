#pragma once

#include "EASTL/string.h"

#include "runtime/function/ui/docking/dock_auto_hide.h"
#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

class DockDragController final {
 public:
  static constexpr float k_default_drag_threshold = 8.0f;
  static constexpr int k_invalid_auto_hide_tab_index = -2;

  DockDragState state() const { return m_state; }
  bool isActive() const { return m_state != DockDragState::idle; }
  bool isHoveringGuide() const { return m_state == DockDragState::hovering_guide; }
  bool isHoveringHostGuide() const { return m_hovering_host_guide; }
  bool isHoveringAutoHide() const { return m_hovering_auto_hide; }
  bool isCrossContainerEligible() const { return m_cross_container_id != k_invalid_dock_id; }
  bool isPreviewActive() const { return m_preview_active; }
  bool wasCancelled() const { return m_cancelled; }
  bool exceededDragThreshold() const;

  DockId draggedWidgetId() const { return m_dragged_widget_id; }
  DockId sourceContainerId() const { return m_source_container_id; }
  DockId hoveredNodeId() const { return m_hovered_node_id; }
  DockSlot hoveredSlot() const { return m_hovered_slot; }
  DockSlot hoveredHostSlot() const { return m_hovered_host_slot; }
  DockId crossContainerId() const { return m_cross_container_id; }
  DockEdge hoveredAutoHideEdge() const { return m_hovered_auto_hide_edge; }
  int autoHideTabInsertIndex() const { return m_auto_hide_tab_insert_index; }
  const DockRect& previewRect() const { return m_preview_rect; }
  const DockRect& hostPreviewRect() const { return m_host_preview_rect; }
  const DockRect& dragPreviewRect() const { return m_drag_preview_rect; }
  const eastl::string& dragPreviewTitle() const { return m_drag_title; }
  const glm::vec2& pointer() const { return m_pointer; }

  void begin(DockId dragged_widget_id, DockId source_container_id, const glm::vec2& pointer,
             eastl::string title, const DockRect& source_frame_rect, float title_bar_height,
             float threshold_px = k_default_drag_threshold);
  void updatePointer(const glm::vec2& pointer, bool drag_preview_enabled);
  void setHover(DockId node_id, DockSlot slot, const DockRect& preview_rect);
  void setCrossContainerEligible(DockId node_id);
  void clearHover();
  void setHostHover(DockSlot slot, const DockRect& preview_rect);
  void clearHostHover();
  void setAutoHideHover(DockEdge edge, int tab_insert_index);
  void clearAutoHideHover();
  bool markDropped();
  bool markHostDockDropped();
  bool markAutoHideDropped();
  void cancel();
  void reset();

 private:
  void updateDragPreviewRect(float title_bar_height);

  DockDragState m_state{DockDragState::idle};
  DockId m_dragged_widget_id{k_invalid_dock_id};
  DockId m_source_container_id{k_invalid_dock_id};
  DockId m_hovered_node_id{k_invalid_dock_id};
  DockSlot m_hovered_slot{DockSlot::none};
  bool m_hovering_host_guide{false};
  DockSlot m_hovered_host_slot{DockSlot::none};
  DockId m_cross_container_id{k_invalid_dock_id};
  bool m_hovering_auto_hide{false};
  DockEdge m_hovered_auto_hide_edge{DockEdge::left};
  int m_auto_hide_tab_insert_index{k_invalid_auto_hide_tab_index};
  DockRect m_preview_rect{};
  DockRect m_host_preview_rect{};
  DockRect m_drag_preview_rect{};
  DockRect m_source_frame_rect{};
  eastl::string m_drag_title;
  glm::vec2 m_pointer{0.0f, 0.0f};
  glm::vec2 m_drag_start_pointer{0.0f, 0.0f};
  float m_title_bar_height{26.0f};
  float m_threshold_px{k_default_drag_threshold};
  bool m_preview_active{false};
  bool m_cancelled{false};
};

}  // namespace Blunder
