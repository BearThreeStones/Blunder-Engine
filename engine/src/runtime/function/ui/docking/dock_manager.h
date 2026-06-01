#pragma once

#include <memory>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_drag_controller.h"
#include "runtime/function/ui/docking/dock_layout_model.h"
#include "runtime/function/ui/docking/dock_node.h"
#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

class DockWidget;

struct DockLayoutMetrics {
  float tab_bar_height{26.0f};
  float splitter_thickness{6.0f};
  float tab_width{132.0f};
  float edge_slot_fraction{0.25f};
  float guide_size{38.0f};
  float guide_gap{6.0f};
  float new_pane_ratio{0.32f};
  float floating_default_width{320.0f};
  float floating_default_height{220.0f};
  float floating_title_height{22.0f};
  float floating_grip{14.0f};
  float floating_min_width{160.0f};
  float floating_min_height{120.0f};
};

struct DockFloatingInteraction {
  bool active{false};
  bool resizing{false};
  DockId node_id{k_invalid_dock_id};
  glm::vec2 start_pointer{0.0f, 0.0f};
  DockRect start_rect{};
};

class DockManager final {
 public:
  DockManager();

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

  void beginDrag(DockId widget_id, const glm::vec2& pointer);
  void handleDragMove(const glm::vec2& pointer);
  void endDrag();
  void cancelDrag();
  const DockDragController& drag() const { return m_drag; }

  void resizeSplitter(DockId split_node_id, const glm::vec2& pointer);

  void beginFloatingMove(DockId floating_node_id, const glm::vec2& pointer);
  void beginFloatingResize(DockId floating_node_id, const glm::vec2& pointer);
  void updateFloatingInteraction(const glm::vec2& pointer);
  void endFloatingInteraction();
  void bringFloatingToFront(DockId floating_node_id);

  DockLayoutModel buildLayoutModel() const;

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
  void appendGuides(DockId node_id, const DockRect& rect, DockSlot active_slot,
                    DockLayoutModel& model) const;
  void appendFloatingFrame(const std::shared_ptr<DockNode>& node, const DockRect& frame,
                           DockLayoutModel& model) const;

  std::shared_ptr<DockNode> hitTestContainer(const std::shared_ptr<DockNode>& node,
                                             const DockRect& rect,
                                             const glm::vec2& pointer,
                                             DockRect& out_rect) const;
  std::shared_ptr<DockNode> hitTest(const glm::vec2& pointer, DockRect& out_rect) const;
  bool findNodeRect(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                    DockId target_id, DockRect& out_rect) const;
  bool resolveNodeRect(DockId node_id, DockRect& out_rect) const;

  DockSlot computeSlot(const DockRect& rect, const glm::vec2& pointer) const;
  DockRect previewRectForSlot(const DockRect& rect, DockSlot slot) const;

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
  DockRect m_host_rect{};
  DockLayoutMetrics m_metrics{};
  DockDragController m_drag;
  DockFloatingInteraction m_floating_interaction;
};

}
