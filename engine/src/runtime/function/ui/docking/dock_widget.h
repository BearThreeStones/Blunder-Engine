#pragma once

#include <memory>
#include <utility>

#include "EASTL/string.h"

#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

class DockNode;

class DockWidget final {
 public:
  DockWidget(DockId id, eastl::string title, DockPanelKind panel_kind);

  DockId id() const { return m_id; }

  const eastl::string& title() const { return m_title; }
  void setTitle(eastl::string title) { m_title = std::move(title); }

  DockPanelKind panelKind() const { return m_panel_kind; }
  void setPanelKind(DockPanelKind panel_kind) { m_panel_kind = panel_kind; }

  bool closable() const { return m_closable; }
  void setClosable(bool closable) { m_closable = closable; }

  std::shared_ptr<DockNode> ownerContainer() const { return m_owner_container.lock(); }
  void setOwnerContainer(const std::shared_ptr<DockNode>& container) {
    m_owner_container = container;
  }
  void clearOwnerContainer() { m_owner_container.reset(); }

 private:
  DockId m_id{k_invalid_dock_id};
  eastl::string m_title;
  DockPanelKind m_panel_kind{DockPanelKind::custom};
  bool m_closable{true};
  std::weak_ptr<DockNode> m_owner_container;
};

}
