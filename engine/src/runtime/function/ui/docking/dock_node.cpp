#include "runtime/function/ui/docking/dock_node.h"

#include <algorithm>

#include "runtime/function/ui/docking/dock_widget.h"

namespace Blunder {

namespace {
constexpr float k_min_split_ratio = 0.05f;
constexpr float k_max_split_ratio = 0.95f;
}

DockNode::DockNode(DockId id, DockNodeKind kind) : m_id(id), m_kind(kind) {}

std::shared_ptr<DockNode> DockNode::makeContainer(DockId id) {
  return std::shared_ptr<DockNode>(new DockNode(id, DockNodeKind::container));
}

std::shared_ptr<DockNode> DockNode::makeSplit(DockId id, SplitDirection direction,
                                              std::shared_ptr<DockNode> first,
                                              std::shared_ptr<DockNode> second,
                                              float ratio) {
  auto node = std::shared_ptr<DockNode>(new DockNode(id, DockNodeKind::split));
  node->m_split_direction = direction;
  node->m_split_ratio = std::clamp(ratio, k_min_split_ratio, k_max_split_ratio);
  node->m_first = std::move(first);
  node->m_second = std::move(second);
  if (node->m_first) {
    node->m_first->setParent(node);
  }
  if (node->m_second) {
    node->m_second->setParent(node);
  }
  return node;
}

std::shared_ptr<DockNode> DockNode::makeFloating(DockId id, const DockRect& rect,
                                                 std::shared_ptr<DockNode> content) {
  auto node = std::shared_ptr<DockNode>(new DockNode(id, DockNodeKind::floating));
  node->m_floating_rect = rect;
  node->m_floating_content = std::move(content);
  if (node->m_floating_content) {
    node->m_floating_content->setParent(node);
  }
  return node;
}

std::shared_ptr<DockWidget> DockNode::activeWidget() const {
  if (m_widgets.empty()) {
    return nullptr;
  }
  const int index = std::clamp(m_active_index, 0, static_cast<int>(m_widgets.size()) - 1);
  return m_widgets[static_cast<size_t>(index)];
}

void DockNode::clampActiveIndex() {
  if (m_widgets.empty()) {
    m_active_index = 0;
    return;
  }
  m_active_index = std::clamp(m_active_index, 0, static_cast<int>(m_widgets.size()) - 1);
}

void DockNode::setActiveIndex(int index) {
  m_active_index = index;
  clampActiveIndex();
}

void DockNode::setActiveWidget(DockId widget_id) {
  const int index = indexOfWidget(widget_id);
  if (index >= 0) {
    m_active_index = index;
  }
}

void DockNode::addWidget(const std::shared_ptr<DockWidget>& widget, int at) {
  if (!widget) {
    return;
  }
  widget->setOwnerContainer(shared_from_this());
  if (at < 0 || at > static_cast<int>(m_widgets.size())) {
    m_widgets.push_back(widget);
    m_active_index = static_cast<int>(m_widgets.size()) - 1;
    return;
  }
  m_widgets.insert(m_widgets.begin() + at, widget);
  m_active_index = at;
}

std::shared_ptr<DockWidget> DockNode::removeWidgetAt(int index) {
  if (index < 0 || index >= static_cast<int>(m_widgets.size())) {
    return nullptr;
  }
  std::shared_ptr<DockWidget> widget = m_widgets[static_cast<size_t>(index)];
  m_widgets.erase(m_widgets.begin() + index);
  if (widget) {
    widget->clearOwnerContainer();
  }
  if (index < m_active_index) {
    --m_active_index;
  }
  clampActiveIndex();
  return widget;
}

std::shared_ptr<DockWidget> DockNode::takeWidget(DockId widget_id) {
  return removeWidgetAt(indexOfWidget(widget_id));
}

int DockNode::indexOfWidget(DockId widget_id) const {
  for (size_t i = 0; i < m_widgets.size(); ++i) {
    if (m_widgets[i] && m_widgets[i]->id() == widget_id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void DockNode::setSplitRatio(float ratio) {
  m_split_ratio = std::clamp(ratio, k_min_split_ratio, k_max_split_ratio);
}

std::shared_ptr<DockNode> DockNode::siblingOf(const DockNode* child) const {
  if (m_first.get() == child) {
    return m_second;
  }
  if (m_second.get() == child) {
    return m_first;
  }
  return nullptr;
}

void DockNode::replaceChild(const DockNode* old_child,
                            const std::shared_ptr<DockNode>& new_child) {
  if (m_first.get() == old_child) {
    m_first = new_child;
    if (m_first) {
      m_first->setParent(shared_from_this());
    }
    return;
  }
  if (m_second.get() == old_child) {
    m_second = new_child;
    if (m_second) {
      m_second->setParent(shared_from_this());
    }
  }
}

void DockNode::setFloatingContent(const std::shared_ptr<DockNode>& content) {
  m_floating_content = content;
  if (m_floating_content) {
    m_floating_content->setParent(shared_from_this());
  }
}

}
