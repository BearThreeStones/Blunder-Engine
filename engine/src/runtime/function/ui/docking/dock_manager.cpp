#include "runtime/function/ui/docking/dock_manager.h"

#include <algorithm>
#include <utility>

#include "EASTL/algorithm.h"

#include "runtime/function/ui/docking/dock_auto_hide.h"
#include "runtime/function/ui/docking/dock_floating.h"
#include "runtime/function/ui/docking/dock_guide_hit.h"
#include "runtime/function/ui/docking/dock_widget.h"

namespace Blunder {

DockManager::DockManager(DockAutoHideFlag auto_hide_config)
    : m_auto_hide_config(auto_hide_config) {
  m_root = DockNode::makeContainer(nextId());
}

void DockManager::setFloatingConfig(DockFloatingFlag config) {
  m_floating_config = config;
  if (!testFloatingFlag(config, DockFloatingFlag::native_os_window)) {
    m_native_floating_ids.clear();
  }
}

bool DockManager::isNativeFloating(DockId node_id) const {
  return m_native_floating_ids.find(node_id) != m_native_floating_ids.end();
}

std::shared_ptr<DockWidget> DockManager::createWidget(eastl::string title,
                                                      DockPanelKind panel_kind) {
  return std::make_shared<DockWidget>(nextId(), std::move(title), panel_kind);
}

void DockManager::dockToRoot(const std::shared_ptr<DockWidget>& widget, DockSlot slot) {
  if (!widget) {
    return;
  }
  if (!m_root) {
    m_root = DockNode::makeContainer(nextId());
  }
  if (m_root->isContainer() &&
      (m_root->isEmptyContainer() || slot == DockSlot::center || slot == DockSlot::none)) {
    m_root->addWidget(widget);
    return;
  }
  const DockSlot effective =
      (slot == DockSlot::center || slot == DockSlot::none) ? DockSlot::right : slot;
  auto new_container = DockNode::makeContainer(nextId());
  new_container->addWidget(widget);
  insertSplit(m_root, effective, new_container);
}

void DockManager::dockWidget(DockId target_node_id, DockSlot slot,
                             const std::shared_ptr<DockWidget>& widget) {
  if (!widget) {
    return;
  }
  auto target = findNode(target_node_id);
  if (!target) {
    dockToRoot(widget, slot);
    return;
  }

  auto source = widget->ownerContainer();

  const bool center_drop =
      (slot == DockSlot::center || slot == DockSlot::none) && target->isContainer();
  if (center_drop) {
    if (source == target) {
      target->setActiveWidget(widget->id());
      return;
    }
    if (source) {
      source->takeWidget(widget->id());
    }
    target->addWidget(widget);
    if (source && source != target && source->isEmptyContainer()) {
      collapseEmptyContainer(source);
    }
    return;
  }

  DockSlot edge = slot;
  if (edge == DockSlot::center || edge == DockSlot::none) {
    edge = DockSlot::right;
  }

  if (source == target && target->isContainer() && target->widgetCount() == 1) {
    return;
  }

  auto new_container = DockNode::makeContainer(nextId());
  if (source) {
    source->takeWidget(widget->id());
  }
  new_container->addWidget(widget);
  insertSplit(target, edge, new_container);
  if (source && source != target && source->isEmptyContainer()) {
    collapseEmptyContainer(source);
  }
}

std::shared_ptr<DockWidget> DockManager::detachWidget(DockId widget_id) {
  auto widget = findWidget(widget_id);
  if (!widget) {
    return nullptr;
  }
  auto container = widget->ownerContainer();
  if (!container) {
    return widget;
  }
  auto taken = container->takeWidget(widget_id);
  if (container->isEmptyContainer()) {
    collapseEmptyContainer(container);
  }
  return taken;
}

void DockManager::closeWidget(DockId widget_id) {
  if (auto* entry = findAutoHideEntry(widget_id)) {
    if (testAutoHideFlag(m_auto_hide_config, DockAutoHideFlag::close_button_collapses)) {
      collapseAutoHide(widget_id);
      return;
    }
    unpinAutoHide(widget_id);
    return;
  }
  detachWidget(widget_id);
}

void DockManager::activateWidget(DockId widget_id) {
  auto container = containerOfWidget(widget_id);
  if (container) {
    container->setActiveWidget(widget_id);
  }
}

void DockManager::beginDrag(DockId widget_id, const glm::vec2& pointer) {
  auto widget = findWidget(widget_id);
  if (!widget) {
    return;
  }
  auto container = widget->ownerContainer();
  DockRect frame_rect = makeDockRect(0.0f, 0.0f, m_metrics.floating_default_width,
                                     m_metrics.floating_default_height);
  if (container) {
    DockRect container_rect;
    if (resolveNodeRect(container->id(), container_rect) && container_rect.valid()) {
      frame_rect = container_rect;
    }
  }
  resetSlotStabilizer();
  m_drag.begin(widget_id, container ? container->id() : k_invalid_dock_id, pointer,
               widget->title(), frame_rect, m_metrics.chrome_bar_height,
               DockDragController::k_default_drag_threshold);
}

bool DockManager::dragNeedsGlobalPointerPoll() const {
  if (!m_drag.isActive()) {
    return false;
  }
  if (nativeFloatingNodeForActiveDrag().has_value()) {
    return true;
  }
  return m_drag.exceededDragThreshold();
}

std::optional<DockId> DockManager::nativeFloatingNodeForActiveDrag() const {
  if (!m_drag.isActive()) {
    return std::nullopt;
  }
  const DockId source_container_id = m_drag.sourceContainerId();
  if (source_container_id == k_invalid_dock_id) {
    return std::nullopt;
  }
  for (const auto& floating_node : m_floating_nodes) {
    if (!floating_node || !isNativeFloating(floating_node->id())) {
      continue;
    }
    const auto& content = floating_node->floatingContent();
    if (content && content->id() == source_container_id) {
      return floating_node->id();
    }
  }
  return std::nullopt;
}

void DockManager::handleDragMove(const glm::vec2& pointer) {
  if (!m_drag.isActive()) {
    return;
  }
  const bool drag_preview_enabled =
      testFloatingFlag(m_floating_config, DockFloatingFlag::drag_preview);
  m_drag.updatePointer(pointer, drag_preview_enabled);

  if (m_host_rect.contains(pointer)) {
    if (const std::optional<DockSlot> host_slot = hitHostGuideSlot(
            m_host_rect, m_metrics.auto_hide_guide_width, m_metrics.auto_hide_guide_height,
            m_metrics.auto_hide_guide_arrow_size, m_metrics.auto_hide_guide_arrow_gap, pointer)) {
      const DockRect preview =
          previewRectForHostSlot(m_host_rect, *host_slot);
      m_drag.setHostHover(*host_slot, preview);
      return;
    }
    m_drag.clearHostHover();
  } else {
    m_drag.clearHostHover();
  }

  if (draggedWidgetCanAutoHide() && m_host_rect.contains(pointer) &&
      !m_drag.isHoveringHostGuide()) {
    const std::optional<DockEdge> auto_hide_edge = computeAutoHideDropEdge(
        m_host_rect, pointer, m_metrics.auto_hide_drop_preview_size,
        m_metrics.auto_hide_strip_thickness, countAutoHideOnEdge(DockEdge::left),
        countAutoHideOnEdge(DockEdge::right), countAutoHideOnEdge(DockEdge::top),
        countAutoHideOnEdge(DockEdge::bottom));
    if (auto_hide_edge.has_value()) {
      const int tab_index = hitAutoHideTabInsertIndex(*auto_hide_edge, pointer);
      m_drag.setAutoHideHover(*auto_hide_edge, tab_index);
    } else {
      m_drag.clearAutoHideHover();
    }
  } else if (!m_drag.isHoveringHostGuide()) {
    m_drag.clearAutoHideHover();
  }

  if (m_drag.isHoveringHostGuide() || m_drag.isHoveringAutoHide()) {
    m_drag.clearHover();
    return;
  }

  if (isInsideOuterMargin(m_host_rect, pointer, m_metrics.dock_outer_margin)) {
    m_drag.clearHover();
    return;
  }

  DockRect target_rect;
  auto target = hitTest(pointer, target_rect);
  if (target && target->isContainer()) {
    const float chrome_h = std::min(m_metrics.chrome_bar_height, target_rect.height);
    const DockRect content_rect = makeDockRect(
        target_rect.x, target_rect.y + chrome_h, target_rect.width,
        std::max(0.0f, target_rect.height - chrome_h));
    if (!content_rect.valid() || !content_rect.contains(pointer)) {
      m_drag.clearHover();
      return;
    }
    if (const std::optional<DockSlot> cross_slot = hitCrossGuideSlot(
            content_rect, m_metrics.guide_size, m_metrics.guide_gap, pointer)) {
      const DockRect preview = previewRectForSlot(content_rect, *cross_slot);
      m_drag.setHover(target->id(), *cross_slot, preview);
      return;
    }
    m_drag.setCrossContainerEligible(target->id());
    return;
  }
  m_drag.clearHover();
}

void DockManager::endDrag() {
  if (!m_drag.isActive()) {
    return;
  }
  if (m_drag.wasCancelled()) {
    resetSlotStabilizer();
    m_drag.reset();
    return;
  }
  if (!m_drag.exceededDragThreshold()) {
    resetSlotStabilizer();
    m_drag.reset();
    return;
  }
  auto widget = findWidget(m_drag.draggedWidgetId());
  const bool host_docked = m_drag.isHoveringHostGuide() && m_drag.markHostDockDropped();
  const bool auto_hide_dropped =
      !host_docked && m_drag.isHoveringAutoHide() && m_drag.markAutoHideDropped();
  const bool split_dropped = !host_docked && !auto_hide_dropped && m_drag.markDropped();
  const bool drag_from_native_float = nativeFloatingNodeForActiveDrag().has_value();
  if (widget) {
    if (host_docked) {
      dockToRoot(widget, m_drag.hoveredHostSlot());
    } else if (auto_hide_dropped) {
      setWidgetAutoHide(widget->id(), m_drag.hoveredAutoHideEdge(), true,
                        m_drag.autoHideTabInsertIndex());
    } else if (split_dropped) {
      dockWidget(m_drag.hoveredNodeId(), m_drag.hoveredSlot(), widget);
    } else if (m_host_rect.contains(m_drag.pointer()) && !drag_from_native_float) {
      makeFloatingFor(widget, m_drag.pointer());
    }
  }
  resetSlotStabilizer();
  m_drag.reset();
}

void DockManager::cancelDrag() {
  if (!m_drag.isActive()) {
    return;
  }
  m_drag.cancel();
  resetSlotStabilizer();
  m_drag.reset();
}

void DockManager::resetSlotStabilizer() {
  m_slot_sticky_valid = false;
  m_last_stabilized_slot = DockSlot::none;
  m_last_slot_pointer = glm::vec2{0.0f, 0.0f};
}

void DockManager::resizeSplitter(DockId split_node_id, const glm::vec2& pointer) {
  auto node = findNode(split_node_id);
  if (!node || !node->isSplit()) {
    return;
  }
  DockRect rect;
  if (!resolveNodeRect(split_node_id, rect) || !rect.valid()) {
    return;
  }
  const float ratio = node->splitDirection() == SplitDirection::horizontal
                          ? (pointer.x - rect.x) / rect.width
                          : (pointer.y - rect.y) / rect.height;
  node->setSplitRatio(ratio);
}

void DockManager::beginFloatingMove(DockId floating_node_id, const glm::vec2& pointer) {
  auto node = findNode(floating_node_id);
  if (!node || !node->isFloating()) {
    return;
  }
  bringFloatingToFront(floating_node_id);
  m_floating_interaction.active = true;
  m_floating_interaction.resizing = false;
  m_floating_interaction.node_id = floating_node_id;
  m_floating_interaction.start_pointer = pointer;
  m_floating_interaction.start_rect = node->floatingRect();
}

void DockManager::beginFloatingResize(DockId floating_node_id, const glm::vec2& pointer,
                                      DockResizeEdge edge) {
  auto node = findNode(floating_node_id);
  if (!node || !node->isFloating()) {
    return;
  }
  bringFloatingToFront(floating_node_id);
  m_floating_interaction.active = true;
  m_floating_interaction.resizing = true;
  m_floating_interaction.node_id = floating_node_id;
  m_floating_interaction.resize_edge = edge;
  m_floating_interaction.start_pointer = pointer;
  m_floating_interaction.start_rect = node->floatingRect();
}

void DockManager::updateFloatingInteraction(const glm::vec2& pointer) {
  if (!m_floating_interaction.active) {
    return;
  }
  auto node = findNode(m_floating_interaction.node_id);
  if (!node || !node->isFloating()) {
    return;
  }
  const glm::vec2 delta = pointer - m_floating_interaction.start_pointer;
  const DockRect& start = m_floating_interaction.start_rect;
  if (m_floating_interaction.resizing) {
    const float min_w = m_metrics.floating_min_width;
    const float min_h = m_metrics.floating_min_height;
    float x = start.x;
    float y = start.y;
    float width = start.width;
    float height = start.height;
    const DockResizeEdge edge = m_floating_interaction.resize_edge;
    const bool resize_east = edge == DockResizeEdge::east ||
                             edge == DockResizeEdge::north_east ||
                             edge == DockResizeEdge::south_east;
    const bool resize_west = edge == DockResizeEdge::west ||
                             edge == DockResizeEdge::north_west ||
                             edge == DockResizeEdge::south_west;
    const bool resize_south = edge == DockResizeEdge::south ||
                              edge == DockResizeEdge::south_east ||
                              edge == DockResizeEdge::south_west;
    const bool resize_north = edge == DockResizeEdge::north ||
                              edge == DockResizeEdge::north_east ||
                              edge == DockResizeEdge::north_west;
    if (resize_east) {
      width = std::max(min_w, start.width + delta.x);
    }
    if (resize_west) {
      width = std::max(min_w, start.width - delta.x);
      x = start.x + start.width - width;
    }
    if (resize_south) {
      height = std::max(min_h, start.height + delta.y);
    }
    if (resize_north) {
      height = std::max(min_h, start.height - delta.y);
      y = start.y + start.height - height;
    }
    if (edge == DockResizeEdge::none) {
      width = std::max(min_w, start.width + delta.x);
      height = std::max(min_h, start.height + delta.y);
    }
    node->setFloatingRect(makeDockRect(x, y, width, height));
    return;
  }
  float x = start.x + delta.x;
  float y = start.y + delta.y;
  if (!isNativeFloating(m_floating_interaction.node_id) && m_host_rect.valid()) {
    const float margin = 40.0f;
    x = std::clamp(x, margin - start.width, m_host_rect.width - margin);
    y = std::clamp(y, 0.0f, std::max(0.0f, m_host_rect.height - m_metrics.chrome_bar_height));
  }
  node->setFloatingRect(makeDockRect(x, y, start.width, start.height));
}

void DockManager::endFloatingInteraction() {
  m_floating_interaction.active = false;
  m_floating_interaction.resizing = false;
  m_floating_interaction.node_id = k_invalid_dock_id;
  m_floating_interaction.resize_edge = DockResizeEdge::none;
}

void DockManager::bringFloatingToFront(DockId floating_node_id) {
  for (size_t i = 0; i < m_floating_nodes.size(); ++i) {
    if (m_floating_nodes[i] && m_floating_nodes[i]->id() == floating_node_id) {
      auto node = m_floating_nodes[i];
      m_floating_nodes.erase(m_floating_nodes.begin() + i);
      m_floating_nodes.push_back(node);
      return;
    }
  }
}

void DockManager::splitChildRects(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                                  DockRect& out_first, DockRect& out_handle,
                                  DockRect& out_second) const {
  const float handle = m_metrics.splitter_thickness;
  const float ratio = node->splitRatio();
  if (node->splitDirection() == SplitDirection::horizontal) {
    const float avail = std::max(0.0f, rect.width - handle);
    const float first_w = avail * ratio;
    const float second_w = avail - first_w;
    out_first = makeDockRect(rect.x, rect.y, first_w, rect.height);
    out_handle = makeDockRect(rect.x + first_w, rect.y, handle, rect.height);
    out_second = makeDockRect(rect.x + first_w + handle, rect.y, second_w, rect.height);
    return;
  }
  const float avail = std::max(0.0f, rect.height - handle);
  const float first_h = avail * ratio;
  const float second_h = avail - first_h;
  out_first = makeDockRect(rect.x, rect.y, rect.width, first_h);
  out_handle = makeDockRect(rect.x, rect.y + first_h, rect.width, handle);
  out_second = makeDockRect(rect.x, rect.y + first_h + handle, rect.width, second_h);
}

void DockManager::solveNode(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                            bool floating, int z_order, DockLayoutModel& model) const {
  if (!node) {
    return;
  }
  if (node->isContainer()) {
    appendContainerTile(node, rect, floating, z_order, model);
    return;
  }
  if (node->isSplit()) {
    DockRect first_rect;
    DockRect handle_rect;
    DockRect second_rect;
    splitChildRects(node, rect, first_rect, handle_rect, second_rect);
    DockSplitterView splitter;
    splitter.node_id = node->id();
    splitter.rect = handle_rect;
    splitter.vertical_handle = node->splitDirection() == SplitDirection::horizontal;
    splitter.ratio = node->splitRatio();
    model.splitters.push_back(splitter);
    solveNode(node->first(), first_rect, floating, z_order, model);
    solveNode(node->second(), second_rect, floating, z_order, model);
    return;
  }
  if (node->isFloating()) {
    if (isNativeFloating(node->id())) {
      return;
    }
    const DockRect frame = node->floatingRect();
    solveNode(node->floatingContent(), frame, true, z_order, model);
    appendFloatingFrame(node, frame, model);
  }
}

void DockManager::appendFloatingFrame(const std::shared_ptr<DockNode>& node,
                                      const DockRect& frame, DockLayoutModel& model) const {
  const float chrome_h = std::min(m_metrics.chrome_bar_height, frame.height);
  const float grip = m_metrics.floating_grip;
  DockFloatingView view;
  view.node_id = node->id();
  view.frame_rect = frame;
  view.title_rect = makeDockRect(frame.x, frame.y, frame.width, chrome_h);
  view.grip_rect =
      makeDockRect(frame.right() - grip, frame.bottom() - grip, grip, grip);
  if (const auto content = node->floatingContent()) {
    if (const auto widget = content->activeWidget()) {
      view.title = widget->title();
    }
  }
  model.floatings.push_back(view);
}

void DockManager::appendContainerTile(const std::shared_ptr<DockNode>& node,
                                      const DockRect& rect, bool floating, int z_order,
                                      DockLayoutModel& model) const {
  const float tab_h = std::min(m_metrics.chrome_bar_height, rect.height);
  DockTileView tile;
  tile.node_id = node->id();
  tile.tab_bar_rect = makeDockRect(rect.x, rect.y, rect.width, tab_h);
  tile.content_rect =
      makeDockRect(rect.x, rect.y + tab_h, rect.width, std::max(0.0f, rect.height - tab_h));
  tile.floating = floating;
  tile.z_order = z_order;
  if (auto active = node->activeWidget()) {
    tile.active_widget_id = active->id();
    tile.active_panel_kind = active->panelKind();
  }
  model.tiles.push_back(tile);

  const auto& widgets = node->widgets();
  if (widgets.empty()) {
    return;
  }
  float tab_w = m_metrics.tab_width;
  const float max_w = rect.width / static_cast<float>(widgets.size());
  if (tab_w > max_w) {
    tab_w = max_w;
  }
  for (size_t i = 0; i < widgets.size(); ++i) {
    const auto& widget = widgets[i];
    if (!widget) {
      continue;
    }
    DockTabView tab;
    tab.widget_id = widget->id();
    tab.container_id = node->id();
    tab.title = widget->title();
    tab.panel_kind = widget->panelKind();
    tab.active = static_cast<int>(i) == node->activeIndex();
    tab.show_pin = testAutoHideFlag(m_auto_hide_config, DockAutoHideFlag::dock_area_has_pin_button) &&
                   hasDockWidgetFeature(widget->features(), DockWidgetFeature::pinnable);
    tab.rect = makeDockRect(rect.x + static_cast<float>(i) * tab_w, rect.y, tab_w, tab_h);
    model.tabs.push_back(tab);
  }
}

void DockManager::appendGuides(DockId node_id, const DockRect& rect, DockSlot active_slot,
                               bool faint, DockLayoutModel& model) const {
  (void)node_id;
  eastl::vector<DockHostGuideLayout> layouts;
  computeCrossGuideLayouts(rect, m_metrics.guide_size, m_metrics.guide_gap, layouts);
  for (const DockHostGuideLayout& layout : layouts) {
    DockGuideView guide;
    guide.slot = layout.slot;
    guide.rect = layout.rect;
    guide.highlighted = layout.slot == active_slot && active_slot != DockSlot::none;
    guide.faint = faint && !guide.highlighted;
    model.guides.push_back(guide);
  }
}

void DockManager::appendHostGuides(const DockRect& host, DockSlot active_slot, bool faint,
                                   DockLayoutModel& model) const {
  eastl::vector<DockHostGuideLayout> layouts;
  computeHostGuideLayouts(host, m_metrics.auto_hide_guide_width, m_metrics.auto_hide_guide_height,
                        m_metrics.auto_hide_guide_arrow_size, m_metrics.auto_hide_guide_arrow_gap,
                        layouts);
  for (const DockHostGuideLayout& layout : layouts) {
    DockHostGuideView guide;
    guide.slot = layout.slot;
    guide.rect = layout.rect;
    guide.highlighted = layout.slot == active_slot && active_slot != DockSlot::none;
    guide.faint = faint && !guide.highlighted;
    model.host_guides.push_back(guide);
  }
}

DockLayoutModel DockManager::buildLayoutModel() const {
  DockLayoutModel model;
  const DockAutoHideInsets insets = computeAutoHideInsets(
      m_host_rect, m_metrics.auto_hide_strip_thickness, countAutoHideOnEdge(DockEdge::left),
      countAutoHideOnEdge(DockEdge::right), countAutoHideOnEdge(DockEdge::top),
      countAutoHideOnEdge(DockEdge::bottom));
  const DockRect inset_host = insetHostRect(m_host_rect, insets);

  if (m_root) {
    solveNode(m_root, inset_host, false, 0, model);
  }
  int z_order = 1;
  for (const auto& floating_node : m_floating_nodes) {
    if (floating_node) {
      solveNode(floating_node, floating_node->floatingRect(), true, z_order++, model);
    }
  }
  appendAutoHideViews(model);
  if (m_drag.isActive()) {
    if (isWithinHostGuideProximity(m_host_rect, m_drag.pointer(),
                                   m_metrics.host_guide_proximity)) {
      const DockSlot active_host_slot =
          m_drag.isHoveringHostGuide() ? m_drag.hoveredHostSlot() : DockSlot::none;
      appendHostGuides(m_host_rect, active_host_slot, true, model);
      if (m_drag.isHoveringHostGuide()) {
        model.host_dock_preview.visible = true;
        model.host_dock_preview.rect = m_drag.hostPreviewRect();
      }
    }
    if (draggedWidgetCanAutoHide() && m_drag.isHoveringAutoHide()) {
      const DockEdge edge = m_drag.hoveredAutoHideEdge();
      model.auto_hide_drop_preview.visible = true;
      model.auto_hide_drop_preview.edge = static_cast<int>(edge);
      model.auto_hide_drop_preview.rect = computeAutoHideDropPreviewRect(
          edge, m_host_rect, m_metrics.auto_hide_drop_preview_size,
          m_metrics.auto_hide_strip_thickness, countAutoHideOnEdge(edge));
    }
    if (m_drag.isCrossContainerEligible()) {
      DockRect target_rect;
      if (resolveNodeRect(m_drag.crossContainerId(), target_rect)) {
        const float chrome_h = std::min(m_metrics.chrome_bar_height, target_rect.height);
        const DockRect content_rect = makeDockRect(
            target_rect.x, target_rect.y + chrome_h, target_rect.width,
            std::max(0.0f, target_rect.height - chrome_h));
        const DockSlot active_slot =
            m_drag.hoveredSlot() != DockSlot::none ? m_drag.hoveredSlot() : DockSlot::none;
        appendGuides(m_drag.crossContainerId(), content_rect, active_slot, true, model);
        model.cross_guides_visible = true;
        if (m_drag.hoveredSlot() != DockSlot::none) {
          model.preview.visible = true;
          model.preview.rect = m_drag.previewRect();
        }
      }
    }
  }
  if (m_drag.isPreviewActive() &&
      testFloatingFlag(m_floating_config, DockFloatingFlag::drag_preview)) {
    model.drag_floating_preview.visible = true;
    model.drag_floating_preview.rect = m_drag.dragPreviewRect();
    model.drag_floating_preview.title = m_drag.dragPreviewTitle();
  }
  return model;
}

std::shared_ptr<DockNode> DockManager::hitTestContainer(const std::shared_ptr<DockNode>& node,
                                                        const DockRect& rect,
                                                        const glm::vec2& pointer,
                                                        DockRect& out_rect) const {
  if (!node) {
    return nullptr;
  }
  if (node->isContainer()) {
    if (rect.contains(pointer)) {
      out_rect = rect;
      return node;
    }
    return nullptr;
  }
  if (node->isSplit()) {
    DockRect first_rect;
    DockRect handle_rect;
    DockRect second_rect;
    splitChildRects(node, rect, first_rect, handle_rect, second_rect);
    if (first_rect.contains(pointer)) {
      return hitTestContainer(node->first(), first_rect, pointer, out_rect);
    }
    if (second_rect.contains(pointer)) {
      return hitTestContainer(node->second(), second_rect, pointer, out_rect);
    }
    return nullptr;
  }
  if (node->isFloating()) {
    return hitTestContainer(node->floatingContent(), node->floatingRect(), pointer, out_rect);
  }
  return nullptr;
}

std::shared_ptr<DockNode> DockManager::hitTest(const glm::vec2& pointer,
                                               DockRect& out_rect) const {
  DockId skip_floating_id = k_invalid_dock_id;
  if (m_drag.isActive()) {
    const DockId source_container_id = m_drag.sourceContainerId();
    for (const auto& floating_node : m_floating_nodes) {
      if (floating_node && floating_node->floatingContent() &&
          floating_node->floatingContent()->id() == source_container_id) {
        skip_floating_id = floating_node->id();
        break;
      }
    }
  }
  for (auto it = m_floating_nodes.rbegin(); it != m_floating_nodes.rend(); ++it) {
    const auto& floating_node = *it;
    if (!floating_node || floating_node->id() == skip_floating_id) {
      continue;
    }
    if (floating_node && floating_node->floatingRect().contains(pointer)) {
      if (auto hit = hitTestContainer(floating_node->floatingContent(),
                                      floating_node->floatingRect(), pointer, out_rect)) {
        return hit;
      }
    }
  }
  if (m_root) {
    return hitTestContainer(m_root, m_host_rect, pointer, out_rect);
  }
  return nullptr;
}

bool DockManager::findNodeRect(const std::shared_ptr<DockNode>& node, const DockRect& rect,
                               DockId target_id, DockRect& out_rect) const {
  if (!node) {
    return false;
  }
  if (node->id() == target_id) {
    out_rect = rect;
    return true;
  }
  if (node->isSplit()) {
    DockRect first_rect;
    DockRect handle_rect;
    DockRect second_rect;
    splitChildRects(node, rect, first_rect, handle_rect, second_rect);
    if (findNodeRect(node->first(), first_rect, target_id, out_rect)) {
      return true;
    }
    return findNodeRect(node->second(), second_rect, target_id, out_rect);
  }
  if (node->isFloating()) {
    return findNodeRect(node->floatingContent(), node->floatingRect(), target_id, out_rect);
  }
  return false;
}

bool DockManager::resolveNodeRect(DockId node_id, DockRect& out_rect) const {
  if (m_root && findNodeRect(m_root, m_host_rect, node_id, out_rect)) {
    return true;
  }
  for (const auto& floating_node : m_floating_nodes) {
    if (floating_node &&
        findNodeRect(floating_node, floating_node->floatingRect(), node_id, out_rect)) {
      return true;
    }
  }
  return false;
}

DockSlot DockManager::computeSlotRaw(const DockRect& rect, const glm::vec2& pointer) const {
  if (!rect.valid()) {
    return DockSlot::center;
  }
  const float fx = (pointer.x - rect.x) / rect.width;
  const float fy = (pointer.y - rect.y) / rect.height;
  const float distances[4] = {fx, 1.0f - fx, fy, 1.0f - fy};
  const DockSlot slots[4] = {DockSlot::left, DockSlot::right, DockSlot::top, DockSlot::bottom};
  int min_idx = 0;
  for (int i = 1; i < 4; ++i) {
    if (distances[i] < distances[min_idx]) {
      min_idx = i;
    }
  }
  if (distances[min_idx] > m_metrics.edge_slot_fraction) {
    return DockSlot::center;
  }
  return slots[min_idx];
}

DockSlot DockManager::stabilizeSlot(const DockRect& rect, const glm::vec2& pointer,
                                    DockSlot raw_slot) {
  if (!rect.valid()) {
    return raw_slot;
  }
  if (!m_slot_sticky_valid) {
    m_slot_sticky_valid = true;
    m_last_stabilized_slot = raw_slot;
    m_last_slot_pointer = pointer;
    return raw_slot;
  }
  if (raw_slot == m_last_stabilized_slot) {
    m_last_slot_pointer = pointer;
    return raw_slot;
  }

  const float fx = (pointer.x - rect.x) / rect.width;
  const float fy = (pointer.y - rect.y) / rect.height;
  const float min_edge = eastl::min(eastl::min(fx, 1.0f - fx), eastl::min(fy, 1.0f - fy));
  const float threshold = m_metrics.edge_slot_fraction;
  const float hysteresis = m_metrics.slot_hysteresis_fraction;

  if (m_last_stabilized_slot == DockSlot::center && raw_slot != DockSlot::center &&
      min_edge > threshold - hysteresis) {
    return m_last_stabilized_slot;
  }
  if (m_last_stabilized_slot != DockSlot::center && raw_slot == DockSlot::center &&
      min_edge < threshold + hysteresis) {
    return m_last_stabilized_slot;
  }

  const glm::vec2 delta = pointer - m_last_slot_pointer;
  const float distance_sq = delta.x * delta.x + delta.y * delta.y;
  const float hysteresis_px = m_metrics.slot_hysteresis_px;
  if (distance_sq < hysteresis_px * hysteresis_px) {
    return m_last_stabilized_slot;
  }

  m_last_stabilized_slot = raw_slot;
  m_last_slot_pointer = pointer;
  return raw_slot;
}

DockRect DockManager::previewRectForHostSlot(const DockRect& host, DockSlot slot) const {
  return Blunder::previewRectForHostSlot(host, slot, m_metrics.host_dock_preview_fraction);
}

DockRect DockManager::previewRectForSlot(const DockRect& rect, DockSlot slot) const {
  const float third_w = rect.width / 3.0f;
  const float third_h = rect.height / 3.0f;
  switch (slot) {
    case DockSlot::left:
      return makeDockRect(rect.x, rect.y, third_w, rect.height);
    case DockSlot::right:
      return makeDockRect(rect.right() - third_w, rect.y, third_w, rect.height);
    case DockSlot::top:
      return makeDockRect(rect.x, rect.y, rect.width, third_h);
    case DockSlot::bottom:
      return makeDockRect(rect.x, rect.bottom() - third_h, rect.width, third_h);
    case DockSlot::center:
    case DockSlot::none:
    default:
      return rect;
  }
}

std::shared_ptr<DockNode> DockManager::containerOfWidget(DockId widget_id) const {
  auto widget = findWidget(widget_id);
  return widget ? widget->ownerContainer() : nullptr;
}

std::shared_ptr<DockNode> DockManager::findNodeRecursive(const std::shared_ptr<DockNode>& node,
                                                         DockId node_id) const {
  if (!node) {
    return nullptr;
  }
  if (node->id() == node_id) {
    return node;
  }
  if (node->isSplit()) {
    if (auto found = findNodeRecursive(node->first(), node_id)) {
      return found;
    }
    return findNodeRecursive(node->second(), node_id);
  }
  if (node->isFloating()) {
    return findNodeRecursive(node->floatingContent(), node_id);
  }
  return nullptr;
}

std::shared_ptr<DockNode> DockManager::findNode(DockId node_id) const {
  if (auto found = findNodeRecursive(m_root, node_id)) {
    return found;
  }
  for (const auto& floating_node : m_floating_nodes) {
    if (auto found = findNodeRecursive(floating_node, node_id)) {
      return found;
    }
  }
  return nullptr;
}

std::shared_ptr<DockWidget> DockManager::findWidgetRecursive(
    const std::shared_ptr<DockNode>& node, DockId widget_id) const {
  if (!node) {
    return nullptr;
  }
  if (node->isContainer()) {
    const int index = node->indexOfWidget(widget_id);
    if (index >= 0) {
      return node->widgets()[static_cast<size_t>(index)];
    }
    return nullptr;
  }
  if (node->isSplit()) {
    if (auto found = findWidgetRecursive(node->first(), widget_id)) {
      return found;
    }
    return findWidgetRecursive(node->second(), widget_id);
  }
  if (node->isFloating()) {
    return findWidgetRecursive(node->floatingContent(), widget_id);
  }
  return nullptr;
}

std::shared_ptr<DockWidget> DockManager::findWidget(DockId widget_id) const {
  if (auto found = findWidgetRecursive(m_root, widget_id)) {
    return found;
  }
  for (const auto& floating_node : m_floating_nodes) {
    if (auto found = findWidgetRecursive(floating_node, widget_id)) {
      return found;
    }
  }
  if (const DockAutoHideEntry* entry = findAutoHideEntry(widget_id)) {
    return entry->widget;
  }
  return nullptr;
}

void DockManager::insertSplit(const std::shared_ptr<DockNode>& target, DockSlot slot,
                              const std::shared_ptr<DockNode>& new_container) {
  const SplitDirection direction =
      isHorizontalSlot(slot) ? SplitDirection::horizontal : SplitDirection::vertical;
  const bool leading = isLeadingSlot(slot);
  const float ratio = leading ? m_metrics.new_pane_ratio : (1.0f - m_metrics.new_pane_ratio);
  auto parent = target->parent();
  auto first = leading ? new_container : target;
  auto second = leading ? target : new_container;
  auto split = DockNode::makeSplit(nextId(), direction, first, second, ratio);
  if (!parent) {
    m_root = split;
    split->clearParent();
    return;
  }
  if (parent->isSplit()) {
    parent->replaceChild(target.get(), split);
    return;
  }
  if (parent->isFloating()) {
    parent->setFloatingContent(split);
  }
}

void DockManager::replaceNodeInParent(const std::shared_ptr<DockNode>& old_node,
                                      const std::shared_ptr<DockNode>& new_node) {
  auto parent = old_node->parent();
  if (!parent) {
    m_root = new_node;
    if (new_node) {
      new_node->clearParent();
    }
    return;
  }
  if (parent->isSplit()) {
    parent->replaceChild(old_node.get(), new_node);
    return;
  }
  if (parent->isFloating()) {
    parent->setFloatingContent(new_node);
  }
}

void DockManager::collapseEmptyContainer(const std::shared_ptr<DockNode>& container) {
  if (!container || !container->isEmptyContainer()) {
    return;
  }
  auto parent = container->parent();
  if (!parent) {
    return;
  }
  if (parent->isSplit()) {
    promoteSibling(parent, container);
    return;
  }
  if (parent->isFloating()) {
    removeFloatingNode(parent);
  }
}

void DockManager::promoteSibling(const std::shared_ptr<DockNode>& split_node,
                                 const std::shared_ptr<DockNode>& empty_child) {
  auto sibling = split_node->siblingOf(empty_child.get());
  if (!sibling) {
    return;
  }
  replaceNodeInParent(split_node, sibling);
}

void DockManager::removeFloatingNode(const std::shared_ptr<DockNode>& floating_node) {
  if (floating_node) {
    m_native_floating_ids.erase(floating_node->id());
  }
  for (auto it = m_floating_nodes.begin(); it != m_floating_nodes.end(); ++it) {
    if (*it == floating_node) {
      m_floating_nodes.erase(it);
      return;
    }
  }
}

std::shared_ptr<DockNode> DockManager::makeFloatingFor(
    const std::shared_ptr<DockWidget>& widget, const glm::vec2& pointer) {
  auto source = widget->ownerContainer();
  if (source) {
    source->takeWidget(widget->id());
    if (source->isEmptyContainer()) {
      collapseEmptyContainer(source);
    }
  }
  auto container = DockNode::makeContainer(nextId());
  container->addWidget(widget);
  const float width = m_metrics.floating_default_width;
  const float height = m_metrics.floating_default_height;
  const DockRect rect = makeDockRect(pointer.x - width * 0.5f,
                                     pointer.y - m_metrics.chrome_bar_height * 0.5f, width, height);
  auto floating = DockNode::makeFloating(nextId(), rect, container);
  m_floating_nodes.push_back(floating);
  if (testFloatingFlag(m_floating_config, DockFloatingFlag::native_os_window) &&
      widget->panelKind() != DockPanelKind::viewport) {
    m_native_floating_ids.insert(floating->id());
  }
  return floating;
}

int DockManager::countAutoHideOnEdge(DockEdge edge) const {
  int count = 0;
  for (const DockAutoHideEntry& entry : m_auto_hide_entries) {
    if (entry.edge == edge) {
      ++count;
    }
  }
  return count;
}

DockAutoHideEntry* DockManager::findAutoHideEntry(DockId widget_id) {
  for (DockAutoHideEntry& entry : m_auto_hide_entries) {
    if (entry.widget_id == widget_id) {
      return &entry;
    }
  }
  return nullptr;
}

const DockAutoHideEntry* DockManager::findAutoHideEntry(DockId widget_id) const {
  for (const DockAutoHideEntry& entry : m_auto_hide_entries) {
    if (entry.widget_id == widget_id) {
      return &entry;
    }
  }
  return nullptr;
}

DockRestoreHint DockManager::captureRestoreHint(
    const std::shared_ptr<DockWidget>& widget) const {
  DockRestoreHint hint{};
  if (!widget) {
    return hint;
  }
  const auto container = widget->ownerContainer();
  if (!container) {
    return hint;
  }
  hint.container_id = container->id();
  hint.tab_index = container->indexOfWidget(widget->id());
  hint.valid = true;
  return hint;
}

bool DockManager::restoreWidgetFromHint(const std::shared_ptr<DockWidget>& widget,
                                        const DockRestoreHint& hint,
                                        DockEdge fallback_edge) {
  if (!widget) {
    return false;
  }
  widget->setAutoHideState(false, fallback_edge);
  if (hint.valid) {
    if (auto container = findNode(hint.container_id)) {
      if (container->isContainer()) {
        container->addWidget(widget, hint.tab_index);
        return true;
      }
    }
  }
  dockToRoot(widget, edgeToDockSlot(fallback_edge));
  return true;
}

void DockManager::collapseOtherExpandedOnEdge(DockEdge edge, DockId except_widget_id) {
  for (DockAutoHideEntry& entry : m_auto_hide_entries) {
    if (entry.edge == edge && entry.widget_id != except_widget_id) {
      entry.expanded = false;
    }
  }
}

void DockManager::appendAutoHideViews(DockLayoutModel& model) const {
  if (!testAutoHideFlag(m_auto_hide_config, DockAutoHideFlag::feature_enabled)) {
    return;
  }
  const float strip = m_metrics.auto_hide_strip_thickness;
  const float tab_span = m_metrics.auto_hide_tab_span;
  const int edge_counts[] = {countAutoHideOnEdge(DockEdge::left),
                             countAutoHideOnEdge(DockEdge::right),
                             countAutoHideOnEdge(DockEdge::top),
                             countAutoHideOnEdge(DockEdge::bottom)};
  int edge_indices[] = {0, 0, 0, 0};

  for (const DockAutoHideEntry& entry : m_auto_hide_entries) {
    if (!entry.widget) {
      continue;
    }
    const int edge_index = static_cast<int>(entry.edge);
    const int index_on_edge = edge_indices[edge_index]++;
    const int count_on_edge = edge_counts[edge_index];

    DockAutoHideTabView tab_view{};
    tab_view.widget_id = entry.widget_id;
    tab_view.edge = edge_index;
    tab_view.title = entry.widget->title();
    tab_view.panel_kind = entry.widget->panelKind();
    tab_view.expanded = entry.expanded;
    tab_view.rect =
        computeAutoHideTabRect(entry.edge, index_on_edge, count_on_edge, m_host_rect, strip, tab_span);
    model.auto_hide_tabs.push_back(tab_view);

    if (!entry.expanded) {
      continue;
    }

    const DockRect content_frame =
        computeAutoHideOverlayContentRect(entry.edge, m_host_rect, strip, entry.expanded_span);
    const DockRect title_rect =
        computeAutoHideOverlayTitleRect(content_frame, m_metrics.auto_hide_title_height);
    const DockRect panel_content = makeDockRect(
        content_frame.x, title_rect.bottom(), content_frame.width,
        std::max(0.0f, content_frame.height - title_rect.height));
    const DockRect resize_rect = computeAutoHideResizeHandleRect(
        entry.edge, content_frame, m_metrics.auto_hide_resize_thickness);

    DockAutoHideOverlayView overlay{};
    overlay.widget_id = entry.widget_id;
    overlay.edge = edge_index;
    overlay.title = entry.widget->title();
    overlay.panel_kind = entry.widget->panelKind();
    overlay.frame_rect = content_frame;
    overlay.title_rect = title_rect;
    overlay.content_rect = panel_content;
    overlay.resize_rect = resize_rect;
    overlay.show_close =
        testAutoHideFlag(m_auto_hide_config, DockAutoHideFlag::has_close_button);
    overlay.show_minimize =
        testAutoHideFlag(m_auto_hide_config, DockAutoHideFlag::has_minimize_button);
    model.auto_hide_overlays.push_back(overlay);
  }
}

bool DockManager::setWidgetAutoHide(DockId widget_id, DockEdge edge, bool enable,
                                    int insert_index_on_edge) {
  if (!testAutoHideFlag(m_auto_hide_config, DockAutoHideFlag::feature_enabled)) {
    return false;
  }
  if (!enable) {
    return unpinAutoHide(widget_id);
  }
  if (auto* existing = findAutoHideEntry(widget_id)) {
    if (existing->edge == edge && insert_index_on_edge >= 0) {
      DockAutoHideEntry moved = *existing;
      m_auto_hide_entries.erase(
          eastl::remove_if(m_auto_hide_entries.begin(), m_auto_hide_entries.end(),
                           [widget_id](const DockAutoHideEntry& e) {
                             return e.widget_id == widget_id;
                           }),
          m_auto_hide_entries.end());
      insertAutoHideEntryAt(moved, edge, insert_index_on_edge);
    }
    return true;
  }
  auto widget = findWidget(widget_id);
  if (!widget || widget->isAutoHide()) {
    return false;
  }
  if (!hasDockWidgetFeature(widget->features(), DockWidgetFeature::pinnable)) {
    return false;
  }

  DockAutoHideEntry entry{};
  entry.widget_id = widget_id;
  entry.widget = widget;
  entry.edge = edge;
  entry.expanded = false;
  entry.expanded_span = isVerticalEdge(edge) ? m_metrics.auto_hide_default_span
                                               : m_metrics.auto_hide_default_span;
  entry.restore = captureRestoreHint(widget);
  detachWidget(widget_id);
  widget->setAutoHideState(true, edge);
  if (insert_index_on_edge < 0) {
    m_auto_hide_entries.push_back(entry);
  } else {
    insertAutoHideEntryAt(entry, edge, insert_index_on_edge);
  }
  return true;
}

bool DockManager::pinWidgetToDefaultEdge(DockId widget_id) {
  auto widget = findWidget(widget_id);
  if (!widget) {
    return false;
  }
  return setWidgetAutoHide(widget_id, defaultAutoHideEdgeForPanel(widget->panelKind()), true);
}

bool DockManager::unpinAutoHide(DockId widget_id) {
  auto* entry = findAutoHideEntry(widget_id);
  if (!entry || !entry->widget) {
    return false;
  }
  const DockEdge edge = entry->edge;
  const DockRestoreHint restore = entry->restore;
  auto widget = entry->widget;
  m_auto_hide_entries.erase(
      eastl::remove_if(m_auto_hide_entries.begin(), m_auto_hide_entries.end(),
                       [widget_id](const DockAutoHideEntry& e) {
                         return e.widget_id == widget_id;
                       }),
      m_auto_hide_entries.end());
  return restoreWidgetFromHint(widget, restore, edge);
}

void DockManager::toggleAutoHideExpanded(DockId widget_id) {
  auto* entry = findAutoHideEntry(widget_id);
  if (!entry) {
    return;
  }
  if (entry->expanded) {
    entry->expanded = false;
    return;
  }
  collapseOtherExpandedOnEdge(entry->edge, widget_id);
  entry->expanded = true;
}

void DockManager::collapseAutoHide(DockId widget_id) {
  if (auto* entry = findAutoHideEntry(widget_id)) {
    entry->expanded = false;
  }
}

void DockManager::collapseAllAutoHide() {
  for (DockAutoHideEntry& entry : m_auto_hide_entries) {
    entry.expanded = false;
  }
}

void DockManager::expandAutoHide(DockId widget_id) {
  auto* entry = findAutoHideEntry(widget_id);
  if (!entry) {
    return;
  }
  collapseOtherExpandedOnEdge(entry->edge, widget_id);
  entry->expanded = true;
}

void DockManager::beginAutoHideResize(DockId widget_id, const glm::vec2& pointer) {
  auto* entry = findAutoHideEntry(widget_id);
  if (!entry || !entry->expanded) {
    return;
  }
  m_auto_hide_resize.active = true;
  m_auto_hide_resize.widget_id = widget_id;
  m_auto_hide_resize.start_pointer = pointer;
  m_auto_hide_resize.start_span = entry->expanded_span;
}

void DockManager::updateAutoHideResize(const glm::vec2& pointer) {
  if (!m_auto_hide_resize.active) {
    return;
  }
  auto* entry = findAutoHideEntry(m_auto_hide_resize.widget_id);
  if (!entry) {
    return;
  }
  const DockAutoHideInsets insets = computeAutoHideInsets(
      m_host_rect, m_metrics.auto_hide_strip_thickness, countAutoHideOnEdge(DockEdge::left),
      countAutoHideOnEdge(DockEdge::right), countAutoHideOnEdge(DockEdge::top),
      countAutoHideOnEdge(DockEdge::bottom));
  const DockRect inset_host = insetHostRect(m_host_rect, insets);
  const glm::vec2 delta = pointer - m_auto_hide_resize.start_pointer;
  float requested = m_auto_hide_resize.start_span;
  if (isVerticalEdge(entry->edge)) {
    requested += (entry->edge == DockEdge::left) ? delta.x : -delta.x;
  } else {
    requested += (entry->edge == DockEdge::top) ? delta.y : -delta.y;
  }
  entry->expanded_span = clampAutoHideExpandedSpan(
      entry->edge, requested, inset_host, m_metrics.auto_hide_strip_thickness,
      m_metrics.auto_hide_min_span, m_metrics.auto_hide_max_fraction);
}

void DockManager::endAutoHideResize() {
  m_auto_hide_resize.active = false;
  m_auto_hide_resize.widget_id = k_invalid_dock_id;
}

bool DockManager::hitTestExpandedAutoHideOverlay(const glm::vec2& pointer,
                                                 DockRect& out_rect) const {
  const DockLayoutModel model = buildLayoutModel();
  for (const DockAutoHideOverlayView& overlay : model.auto_hide_overlays) {
    if (overlay.frame_rect.contains(pointer)) {
      out_rect = overlay.frame_rect;
      return true;
    }
  }
  return false;
}

bool DockManager::draggedWidgetCanAutoHide() const {
  if (!m_drag.isActive() ||
      !testAutoHideFlag(m_auto_hide_config, DockAutoHideFlag::feature_enabled)) {
    return false;
  }
  auto widget = findWidget(m_drag.draggedWidgetId());
  return widget && hasDockWidgetFeature(widget->features(), DockWidgetFeature::pinnable);
}

int DockManager::hitAutoHideTabInsertIndex(DockEdge edge, const glm::vec2& pointer) const {
  const int count_on_edge = countAutoHideOnEdge(edge);
  if (count_on_edge <= 0) {
    return 0;
  }

  const float strip = m_metrics.auto_hide_strip_thickness;
  const float tab_span = m_metrics.auto_hide_tab_span;
  int index_on_edge = 0;
  for (const DockAutoHideEntry& entry : m_auto_hide_entries) {
    if (entry.edge != edge) {
      continue;
    }
    const DockRect tab_rect = computeAutoHideTabRect(edge, index_on_edge, count_on_edge,
                                                     m_host_rect, strip, tab_span);
    const DockRect host_tab =
        makeDockRect(m_host_rect.x + tab_rect.x, m_host_rect.y + tab_rect.y, tab_rect.width,
                     tab_rect.height);
    if (isVerticalEdge(edge)) {
      if (pointer.y < host_tab.y + host_tab.height * 0.5f) {
        return index_on_edge;
      }
    } else if (pointer.x < host_tab.x + host_tab.width * 0.5f) {
      return index_on_edge;
    }
    ++index_on_edge;
  }
  return count_on_edge;
}

void DockManager::insertAutoHideEntryAt(DockAutoHideEntry entry, DockEdge edge,
                                        int insert_index_on_edge) {
  const int clamped_index = std::max(0, insert_index_on_edge);
  size_t insert_at = m_auto_hide_entries.size();
  int edge_index = 0;
  for (size_t i = 0; i < m_auto_hide_entries.size(); ++i) {
    if (m_auto_hide_entries[i].edge != edge) {
      continue;
    }
    if (edge_index == clamped_index) {
      insert_at = i;
      break;
    }
    ++edge_index;
  }
  m_auto_hide_entries.insert(m_auto_hide_entries.begin() + static_cast<ptrdiff_t>(insert_at),
                             entry);
}

}
