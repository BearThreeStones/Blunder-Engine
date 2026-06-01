#include "runtime/function/ui/docking/dock_manager.h"

#include <algorithm>
#include <utility>

#include "runtime/function/ui/docking/dock_widget.h"

namespace Blunder {

DockManager::DockManager() { m_root = DockNode::makeContainer(nextId()); }

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

void DockManager::closeWidget(DockId widget_id) { detachWidget(widget_id); }

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
  m_drag.begin(widget_id, container ? container->id() : k_invalid_dock_id, pointer);
}

void DockManager::handleDragMove(const glm::vec2& pointer) {
  if (!m_drag.isActive()) {
    return;
  }
  m_drag.updatePointer(pointer);
  DockRect target_rect;
  auto target = hitTest(pointer, target_rect);
  if (target && target->isContainer()) {
    const DockSlot slot = computeSlot(target_rect, pointer);
    const DockRect preview = previewRectForSlot(target_rect, slot);
    m_drag.setHover(target->id(), slot, preview);
    return;
  }
  m_drag.clearHover();
}

void DockManager::endDrag() {
  if (!m_drag.isActive()) {
    return;
  }
  auto widget = findWidget(m_drag.draggedWidgetId());
  const bool dropped = m_drag.markDropped();
  if (widget) {
    if (dropped) {
      dockWidget(m_drag.hoveredNodeId(), m_drag.hoveredSlot(), widget);
    } else if (m_host_rect.contains(m_drag.pointer())) {
      makeFloatingFor(widget, m_drag.pointer());
    }
  }
  m_drag.reset();
}

void DockManager::cancelDrag() { m_drag.reset(); }

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

void DockManager::beginFloatingResize(DockId floating_node_id, const glm::vec2& pointer) {
  auto node = findNode(floating_node_id);
  if (!node || !node->isFloating()) {
    return;
  }
  bringFloatingToFront(floating_node_id);
  m_floating_interaction.active = true;
  m_floating_interaction.resizing = true;
  m_floating_interaction.node_id = floating_node_id;
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
    const float width = std::max(m_metrics.floating_min_width, start.width + delta.x);
    const float height = std::max(m_metrics.floating_min_height, start.height + delta.y);
    node->setFloatingRect(makeDockRect(start.x, start.y, width, height));
    return;
  }
  float x = start.x + delta.x;
  float y = start.y + delta.y;
  if (m_host_rect.valid()) {
    const float margin = 40.0f;
    x = std::clamp(x, margin - start.width, m_host_rect.width - margin);
    y = std::clamp(y, 0.0f, std::max(0.0f, m_host_rect.height - m_metrics.floating_title_height));
  }
  node->setFloatingRect(makeDockRect(x, y, start.width, start.height));
}

void DockManager::endFloatingInteraction() {
  m_floating_interaction.active = false;
  m_floating_interaction.resizing = false;
  m_floating_interaction.node_id = k_invalid_dock_id;
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
    const DockRect frame = node->floatingRect();
    const float title_h = std::min(m_metrics.floating_title_height, frame.height);
    const DockRect content_area = makeDockRect(frame.x, frame.y + title_h, frame.width,
                                               std::max(0.0f, frame.height - title_h));
    solveNode(node->floatingContent(), content_area, true, z_order, model);
    appendFloatingFrame(node, frame, model);
  }
}

void DockManager::appendFloatingFrame(const std::shared_ptr<DockNode>& node,
                                      const DockRect& frame, DockLayoutModel& model) const {
  const float title_h = std::min(m_metrics.floating_title_height, frame.height);
  const float grip = m_metrics.floating_grip;
  DockFloatingView view;
  view.node_id = node->id();
  view.frame_rect = frame;
  view.title_rect = makeDockRect(frame.x, frame.y, frame.width, title_h);
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
  const float tab_h = std::min(m_metrics.tab_bar_height, rect.height);
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
    tab.rect = makeDockRect(rect.x + static_cast<float>(i) * tab_w, rect.y, tab_w, tab_h);
    model.tabs.push_back(tab);
  }
}

void DockManager::appendGuides(DockId node_id, const DockRect& rect, DockSlot active_slot,
                               DockLayoutModel& model) const {
  (void)node_id;
  const glm::vec2 center = rect.center();
  const float size = m_metrics.guide_size;
  const float gap = m_metrics.guide_gap;
  const float half = size * 0.5f;
  const float step = size + gap;
  const struct GuideAnchor {
    DockSlot slot;
    float cx;
    float cy;
  } anchors[] = {
      {DockSlot::center, center.x, center.y},
      {DockSlot::left, center.x - step, center.y},
      {DockSlot::right, center.x + step, center.y},
      {DockSlot::top, center.x, center.y - step},
      {DockSlot::bottom, center.x, center.y + step},
  };
  for (const GuideAnchor& anchor : anchors) {
    DockGuideView guide;
    guide.slot = anchor.slot;
    guide.rect = makeDockRect(anchor.cx - half, anchor.cy - half, size, size);
    guide.highlighted = anchor.slot == active_slot;
    model.guides.push_back(guide);
  }
}

DockLayoutModel DockManager::buildLayoutModel() const {
  DockLayoutModel model;
  if (m_root) {
    solveNode(m_root, m_host_rect, false, 0, model);
  }
  int z_order = 1;
  for (const auto& floating_node : m_floating_nodes) {
    if (floating_node) {
      solveNode(floating_node, floating_node->floatingRect(), true, z_order++, model);
    }
  }
  if (m_drag.isHoveringGuide()) {
    DockRect target_rect;
    if (resolveNodeRect(m_drag.hoveredNodeId(), target_rect)) {
      appendGuides(m_drag.hoveredNodeId(), target_rect, m_drag.hoveredSlot(), model);
    }
    model.preview.visible = true;
    model.preview.rect = m_drag.previewRect();
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
  for (auto it = m_floating_nodes.rbegin(); it != m_floating_nodes.rend(); ++it) {
    const auto& floating_node = *it;
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

DockSlot DockManager::computeSlot(const DockRect& rect, const glm::vec2& pointer) const {
  if (!rect.valid()) {
    return DockSlot::center;
  }
  const float fx = (pointer.x - rect.x) / rect.width;
  const float fy = (pointer.y - rect.y) / rect.height;
  const float distance_left = fx;
  const float distance_right = 1.0f - fx;
  const float distance_top = fy;
  const float distance_bottom = 1.0f - fy;
  const float min_edge = std::min(std::min(distance_left, distance_right),
                                  std::min(distance_top, distance_bottom));
  if (min_edge > m_metrics.edge_slot_fraction) {
    return DockSlot::center;
  }
  if (min_edge == distance_left) {
    return DockSlot::left;
  }
  if (min_edge == distance_right) {
    return DockSlot::right;
  }
  if (min_edge == distance_top) {
    return DockSlot::top;
  }
  return DockSlot::bottom;
}

DockRect DockManager::previewRectForSlot(const DockRect& rect, DockSlot slot) const {
  const float half_w = rect.width * 0.5f;
  const float half_h = rect.height * 0.5f;
  switch (slot) {
    case DockSlot::left:
      return makeDockRect(rect.x, rect.y, half_w, rect.height);
    case DockSlot::right:
      return makeDockRect(rect.x + half_w, rect.y, half_w, rect.height);
    case DockSlot::top:
      return makeDockRect(rect.x, rect.y, rect.width, half_h);
    case DockSlot::bottom:
      return makeDockRect(rect.x, rect.y + half_h, rect.width, half_h);
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
                                     pointer.y - m_metrics.tab_bar_height * 0.5f, width, height);
  auto floating = DockNode::makeFloating(nextId(), rect, container);
  m_floating_nodes.push_back(floating);
  return floating;
}

}
