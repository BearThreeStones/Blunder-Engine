#pragma once

#include <memory>
#include <optional>

#include "EASTL/string.h"
#include "EASTL/unordered_set.h"
#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_auto_hide.h"
#include "runtime/function/ui/docking/dock_guide_hit.h"
#include "runtime/function/ui/docking/dock_drag_controller.h"
#include "runtime/function/ui/docking/dock_floating.h"
#include "runtime/function/ui/docking/dock_layout_model.h"
#include "runtime/function/ui/docking/dock_node.h"
#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

class DockWidget;

struct DockLayoutMetrics {
  // Keep in sync with DockChromeMetrics.bar-height in docking_panel.slint
  float chrome_bar_height{32.0f};
  float tab_bar_height{32.0f};
  float splitter_thickness{6.0f};
  float tab_width{132.0f};
  float edge_slot_fraction{0.25f};
  float slot_hysteresis_px{4.0f};
  float slot_hysteresis_fraction{0.02f};
  float guide_size{38.0f};
  float guide_gap{6.0f};
  float new_pane_ratio{0.32f};
  float floating_default_width{320.0f};
  float floating_default_height{220.0f};
  float floating_title_height{22.0f};
  float floating_grip{14.0f};
  float floating_min_width{160.0f};
  float floating_min_height{120.0f};
  float auto_hide_strip_thickness{28.0f};
  float auto_hide_tab_span{72.0f};
  float auto_hide_default_span{280.0f};
  float auto_hide_min_span{64.0f};
  float auto_hide_max_fraction{0.85f};
  float auto_hide_title_height{22.0f};
  float auto_hide_resize_thickness{6.0f};
  // ADS: AutoHideAreaMouseZone = 8 (legacy); effective outer depth uses preview_size below.
  float auto_hide_drop_mouse_zone{8.0f};
  // Default outer hit depth and strip preview width when edge has no tabs (32px).
  float auto_hide_drop_preview_size{32.0f};
  // Landscape icon (top/bottom): width x height. Portrait (left/right) swaps these.
  // Keep in sync with DockEdgeGuideMetrics in docking_panel.slint.
  float auto_hide_guide_width{48.0f};
  float auto_hide_guide_height{28.0f};
  float auto_hide_guide_arrow_size{8.0f};
  float auto_hide_guide_arrow_gap{3.0f};
  // Host-level edge dock guides (ADS outer dock area).
  float dock_outer_margin{24.0f};
  float host_guide_proximity{48.0f};
  float host_dock_preview_fraction{0.5f};
};

struct DockFloatingInteraction {
  bool active{false};
  bool resizing{false};
  DockId node_id{k_invalid_dock_id};
  DockResizeEdge resize_edge{DockResizeEdge::none};
  glm::vec2 start_pointer{0.0f, 0.0f};
  DockRect start_rect{};
};

struct DockAutoHideResizeInteraction {
  bool active{false};
  DockId widget_id{k_invalid_dock_id};
  glm::vec2 start_pointer{0.0f, 0.0f};
  float start_span{0.0f};
};

class DockManager final {
 public:
  explicit DockManager(DockAutoHideFlag auto_hide_config = DockAutoHideFlag::default_config);

  std::shared_ptr<DockWidget> createWidget(eastl::string title, DockPanelKind panel_kind);

  void dockToRoot(const std::shared_ptr<DockWidget>& widget, DockSlot slot);
  void dockWidget(DockId target_node_id, DockSlot slot,
                  const std::shared_ptr<DockWidget>& widget);
  std::shared_ptr<DockWidget> detachWidget(DockId widget_id);
  void closeWidget(DockId widget_id);
  void activateWidget(DockId widget_id);

  const std::shared_ptr<DockNode>& root() const { return m_root; }
  const eastl::vector<std::shared_ptr<DockNode>>& floatingNodes() const {
    return m_floating_nodes;
  }

  void setHostRect(const DockRect& rect) { m_host_rect = rect; }
  const DockRect& hostRect() const { return m_host_rect; }
  void setMetrics(const DockLayoutMetrics& metrics) { m_metrics = metrics; }
  const DockLayoutMetrics& metrics() const { return m_metrics; }

  DockAutoHideFlag autoHideConfig() const { return m_auto_hide_config; }
  void setAutoHideConfig(DockAutoHideFlag config) { m_auto_hide_config = config; }

  DockFloatingFlag floatingConfig() const { return m_floating_config; }
  void setFloatingConfig(DockFloatingFlag config);
  bool isNativeFloating(DockId node_id) const;

  bool setWidgetAutoHide(DockId widget_id, DockEdge edge, bool enable,
                         int insert_index_on_edge = -1);
  bool pinWidgetToDefaultEdge(DockId widget_id);
  bool unpinAutoHide(DockId widget_id);
  void toggleAutoHideExpanded(DockId widget_id);
  void collapseAutoHide(DockId widget_id);
  void collapseAllAutoHide();
  void expandAutoHide(DockId widget_id);

  void beginAutoHideResize(DockId widget_id, const glm::vec2& pointer);
  void updateAutoHideResize(const glm::vec2& pointer);
  void endAutoHideResize();
  bool isAutoHideResizeActive() const { return m_auto_hide_resize.active; }

  void beginDrag(DockId widget_id, const glm::vec2& pointer);
  void handleDragMove(const glm::vec2& pointer);
  void endDrag();
  void cancelDrag();
  const DockDragController& drag() const { return m_drag; }
  bool dragNeedsGlobalPointerPoll() const;
  std::optional<DockId> nativeFloatingNodeForActiveDrag() const;

  void resizeSplitter(DockId split_node_id, const glm::vec2& pointer);

  void beginFloatingMove(DockId floating_node_id, const glm::vec2& pointer);
  void beginFloatingResize(DockId floating_node_id, const glm::vec2& pointer,
                           DockResizeEdge edge = DockResizeEdge::south_east);
  void updateFloatingInteraction(const glm::vec2& pointer);
  void endFloatingInteraction();
  bool isFloatingInteractionActive() const { return m_floating_interaction.active; }
  void bringFloatingToFront(DockId floating_node_id);

  DockLayoutModel buildLayoutModel() const;
  bool hitTestExpandedAutoHideOverlay(const glm::vec2& pointer, DockRect& out_rect) const;

  std::shared_ptr<DockNode> findNode(DockId node_id) const;
  std::shared_ptr<DockWidget> findWidget(DockId widget_id) const;

 private:
  DockId nextId() { return m_next_id++; }

  void splitChildRects(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                       DockRect& out_first, DockRect& out_handle,
                       DockRect& out_second) const;

  void solveNode(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                 bool floating, int z_order, DockLayoutModel& model) const;
  void appendContainerTile(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                           bool floating, int z_order, DockLayoutModel& model) const;
  void appendAutoHideViews(DockLayoutModel& model) const;
  void appendGuides(DockId node_id, const DockRect& rect, DockSlot active_slot, bool faint,
                    DockLayoutModel& model) const;
  void appendHostGuides(const DockRect& host, DockSlot active_slot, bool faint,
                        DockLayoutModel& model) const;
  void appendFloatingFrame(const std::shared_ptr<DockNode>& node, const DockRect& frame,
                           DockLayoutModel& model) const;

  int countAutoHideOnEdge(DockEdge edge) const;
  DockAutoHideEntry* findAutoHideEntry(DockId widget_id);
  const DockAutoHideEntry* findAutoHideEntry(DockId widget_id) const;
  DockRestoreHint captureRestoreHint(const std::shared_ptr<DockWidget>& widget) const;
  bool restoreWidgetFromHint(const std::shared_ptr<DockWidget>& widget,
                             const DockRestoreHint& hint, DockEdge fallback_edge);
  void collapseOtherExpandedOnEdge(DockEdge edge, DockId except_widget_id);
  bool draggedWidgetCanAutoHide() const;
  int hitAutoHideTabInsertIndex(DockEdge edge, const glm::vec2& pointer) const;
  void insertAutoHideEntryAt(DockAutoHideEntry entry, DockEdge edge, int insert_index_on_edge);

  std::shared_ptr<DockNode> hitTestContainer(const std::shared_ptr<DockNode>& node,
                                             const DockRect& rect,
                                             const glm::vec2& pointer,
                                             DockRect& out_rect) const;
  std::shared_ptr<DockNode> hitTest(const glm::vec2& pointer, DockRect& out_rect) const;
  bool findNodeRect(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                    DockId target_id, DockRect& out_rect) const;
  bool resolveNodeRect(DockId node_id, DockRect& out_rect) const;

  DockSlot computeSlotRaw(const DockRect& rect, const glm::vec2& pointer) const;
  DockSlot stabilizeSlot(const DockRect& rect, const glm::vec2& pointer, DockSlot raw_slot);
  void resetSlotStabilizer();
  DockRect previewRectForSlot(const DockRect& rect, DockSlot slot) const;
  DockRect previewRectForHostSlot(const DockRect& host, DockSlot slot) const;

  std::shared_ptr<DockNode> containerOfWidget(DockId widget_id) const;
  std::shared_ptr<DockNode> findNodeRecursive(const std::shared_ptr<DockNode>& node,
                                              DockId node_id) const;
  std::shared_ptr<DockWidget> findWidgetRecursive(const std::shared_ptr<DockNode>& node,
                                                  DockId widget_id) const;

  void insertSplit(const std::shared_ptr<DockNode>& target, DockSlot slot,
                   const std::shared_ptr<DockNode>& new_container);
  void replaceNodeInParent(const std::shared_ptr<DockNode>& old_node,
                           const std::shared_ptr<DockNode>& new_node);
  void collapseEmptyContainer(const std::shared_ptr<DockNode>& container);
  void promoteSibling(const std::shared_ptr<DockNode>& split_node,
                      const std::shared_ptr<DockNode>& empty_child);
  void removeFloatingNode(const std::shared_ptr<DockNode>& floating_node);
  std::shared_ptr<DockNode> makeFloatingFor(const std::shared_ptr<DockWidget>& widget,
                                            const glm::vec2& pointer);

  DockId m_next_id{1};
  std::shared_ptr<DockNode> m_root;
  eastl::vector<std::shared_ptr<DockNode>> m_floating_nodes;
  eastl::vector<DockAutoHideEntry> m_auto_hide_entries;
  DockRect m_host_rect{};
  DockLayoutMetrics m_metrics{};
  DockAutoHideFlag m_auto_hide_config{DockAutoHideFlag::default_config};
  DockFloatingFlag m_floating_config{DockFloatingFlag::default_config};
  DockDragController m_drag;
  bool m_slot_sticky_valid{false};
  DockSlot m_last_stabilized_slot{DockSlot::none};
  glm::vec2 m_last_slot_pointer{0.0f, 0.0f};
  DockFloatingInteraction m_floating_interaction;
  DockAutoHideResizeInteraction m_auto_hide_resize;
  eastl::unordered_set<DockId> m_native_floating_ids;
};

}  // namespace Blunder
