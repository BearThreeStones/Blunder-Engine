#pragma once

#include <memory>

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_auto_hide.h"
#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

inline const char* defaultDockPanelTitle(DockPanelKind kind) {
  switch (kind) {
    case DockPanelKind::viewport:
      return "Viewport";
    case DockPanelKind::hierarchy:
      return "Hierarchy";
    case DockPanelKind::inspector:
      return "Inspector";
    case DockPanelKind::content_browser:
      return "Content Browser";
    case DockPanelKind::animation:
      return "Animation";
    case DockPanelKind::console:
      return "Console";
    case DockPanelKind::custom:
    default:
      return "Panel";
  }
}

struct DockLayoutNodeSnapshot {
  DockNodeKind kind{DockNodeKind::container};
  SplitDirection split_direction{SplitDirection::none};
  float split_ratio{0.5f};
  eastl::vector<DockPanelKind> widgets;
  int active_index{0};
  std::shared_ptr<DockLayoutNodeSnapshot> first;
  std::shared_ptr<DockLayoutNodeSnapshot> second;
};

struct DockFloatingSnapshot {
  DockRect rect{};
  bool native{false};
  DockLayoutNodeSnapshot content;
};

struct DockAutoHideSnapshot {
  DockPanelKind kind{DockPanelKind::custom};
  DockEdge edge{DockEdge::left};
  bool expanded{false};
  float expanded_span{280.0f};
};

struct DockLayoutSnapshot {
  DockLayoutNodeSnapshot root;
  eastl::vector<DockFloatingSnapshot> floating;
  eastl::vector<DockAutoHideSnapshot> auto_hide;
};

}  // namespace Blunder
