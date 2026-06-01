#pragma once

#include "EASTL/string.h"
#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

struct DockTabView {
  DockId widget_id{k_invalid_dock_id};
  DockId container_id{k_invalid_dock_id};
  eastl::string title;
  DockPanelKind panel_kind{DockPanelKind::custom};
  bool active{false};
  DockRect rect{};
};

struct DockTileView {
  DockId node_id{k_invalid_dock_id};
  DockRect tab_bar_rect{};
  DockRect content_rect{};
  DockId active_widget_id{k_invalid_dock_id};
  DockPanelKind active_panel_kind{DockPanelKind::custom};
  bool floating{false};
  int z_order{0};
};

struct DockSplitterView {
  DockId node_id{k_invalid_dock_id};
  DockRect rect{};
  bool vertical_handle{false};
  float ratio{0.5f};
};

struct DockGuideView {
  DockSlot slot{DockSlot::none};
  DockRect rect{};
  bool highlighted{false};
};

struct DockPreviewView {
  bool visible{false};
  DockRect rect{};
};

struct DockFloatingView {
  DockId node_id{k_invalid_dock_id};
  DockRect frame_rect{};
  DockRect title_rect{};
  DockRect grip_rect{};
  eastl::string title;
};

struct DockLayoutModel {
  eastl::vector<DockTileView> tiles;
  eastl::vector<DockTabView> tabs;
  eastl::vector<DockSplitterView> splitters;
  eastl::vector<DockGuideView> guides;
  eastl::vector<DockFloatingView> floatings;
  DockPreviewView preview{};
};

}
