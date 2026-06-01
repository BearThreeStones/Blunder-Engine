#pragma once

#include <memory>

#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

class DockWidget;

class DockNode final : public std::enable_shared_from_this<DockNode> {
 public:
  static std::shared_ptr<DockNode> makeContainer(DockId id);
  static std::shared_ptr<DockNode> makeSplit(DockId id, SplitDirection direction,
                                             std::shared_ptr<DockNode> first,
                                             std::shared_ptr<DockNode> second,
                                             float ratio);
  static std::shared_ptr<DockNode> makeFloating(DockId id, const DockRect& rect,
                                                std::shared_ptr<DockNode> content);

  DockId id() const { return m_id; }
  DockNodeKind kind() const { return m_kind; }
  bool isContainer() const { return m_kind == DockNodeKind::container; }
  bool isSplit() const { return m_kind == DockNodeKind::split; }
  bool isFloating() const { return m_kind == DockNodeKind::floating; }

  std::shared_ptr<DockNode> parent() const { return m_parent.lock(); }
  void setParent(const std::shared_ptr<DockNode>& parent) { m_parent = parent; }
  void clearParent() { m_parent.reset(); }

  const eastl::vector<std::shared_ptr<DockWidget>>& widgets() const { return m_widgets; }
  int widgetCount() const { return static_cast<int>(m_widgets.size()); }
  bool isEmptyContainer() const { return isContainer() && m_widgets.empty(); }
  int activeIndex() const { return m_active_index; }
  std::shared_ptr<DockWidget> activeWidget() const;
  void setActiveIndex(int index);
  void setActiveWidget(DockId widget_id);
  void addWidget(const std::shared_ptr<DockWidget>& widget, int at = -1);
  std::shared_ptr<DockWidget> removeWidgetAt(int index);
  std::shared_ptr<DockWidget> takeWidget(DockId widget_id);
  int indexOfWidget(DockId widget_id) const;

  SplitDirection splitDirection() const { return m_split_direction; }
  float splitRatio() const { return m_split_ratio; }
  void setSplitRatio(float ratio);
  const std::shared_ptr<DockNode>& first() const { return m_first; }
  const std::shared_ptr<DockNode>& second() const { return m_second; }
  std::shared_ptr<DockNode> siblingOf(const DockNode* child) const;
  void replaceChild(const DockNode* old_child, const std::shared_ptr<DockNode>& new_child);

  const DockRect& floatingRect() const { return m_floating_rect; }
  void setFloatingRect(const DockRect& rect) { m_floating_rect = rect; }
  const std::shared_ptr<DockNode>& floatingContent() const { return m_floating_content; }
  void setFloatingContent(const std::shared_ptr<DockNode>& content);

 private:
  DockNode(DockId id, DockNodeKind kind);

  void clampActiveIndex();

  DockId m_id{k_invalid_dock_id};
  DockNodeKind m_kind{DockNodeKind::container};
  std::weak_ptr<DockNode> m_parent;

  eastl::vector<std::shared_ptr<DockWidget>> m_widgets;
  int m_active_index{0};

  SplitDirection m_split_direction{SplitDirection::none};
  float m_split_ratio{0.5f};
  std::shared_ptr<DockNode> m_first;
  std::shared_ptr<DockNode> m_second;

  DockRect m_floating_rect{};
  std::shared_ptr<DockNode> m_floating_content;
};

}
