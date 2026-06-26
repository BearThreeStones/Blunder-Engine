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
  bool show_pin{false};
  DockRect rect{};
};

struct DockAutoHideTabView {
  DockId widget_id{k_invalid_dock_id};
  int edge{0};
  eastl::string title;
  DockPanelKind panel_kind{DockPanelKind::custom};
  bool expanded{false};
  DockRect rect{};
};

struct DockAutoHideOverlayView {
  DockId widget_id{k_invalid_dock_id};
  int edge{0};
  eastl::string title;
  DockPanelKind panel_kind{DockPanelKind::custom};
  DockRect frame_rect{};
  DockRect title_rect{};
  DockRect content_rect{};
  DockRect resize_rect{};
  bool show_close{false};
  bool show_minimize{false};
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
  bool faint{false};
};

struct DockHostGuideView {
  DockSlot slot{DockSlot::none};
  DockRect rect{};
  bool highlighted{false};
  bool faint{false};
};

struct DockPreviewView {
  bool visible{false};
  DockRect rect{};
};

struct DockAutoHideDropPreviewView {
  bool visible{false};
  int edge{0};
  DockRect rect{};
};

struct DockAutoHideGuideView {
  int edge{0};
  DockRect rect{};
  bool highlighted{false};
};

struct DockFloatingView {
  DockId node_id{k_invalid_dock_id};
  DockRect frame_rect{};
  DockRect title_rect{};
  DockRect grip_rect{};
  eastl::string title;
};

struct DockDragPreviewView {
  bool visible{false};
  DockRect rect{};
  eastl::string title;
};

struct DockLayoutModel {
  eastl::vector<DockTileView> tiles;
  eastl::vector<DockTabView> tabs;
  eastl::vector<DockSplitterView> splitters;
  eastl::vector<DockGuideView> guides;
  eastl::vector<DockHostGuideView> host_guides;
  eastl::vector<DockFloatingView> floatings;
  eastl::vector<DockAutoHideTabView> auto_hide_tabs;
  eastl::vector<DockAutoHideOverlayView> auto_hide_overlays;
  DockPreviewView preview{};
  DockPreviewView host_dock_preview{};
  DockAutoHideDropPreviewView auto_hide_drop_preview{};
  eastl::vector<DockAutoHideGuideView> auto_hide_guides;
  bool auto_hide_guides_visible{false};
  bool cross_guides_visible{false};
  DockDragPreviewView drag_floating_preview{};
};

}
