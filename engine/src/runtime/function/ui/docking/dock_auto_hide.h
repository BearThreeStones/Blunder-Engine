#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

class DockWidget;

enum class DockEdge : uint8_t {
  left = 0,
  right,
  top,
  bottom,
};

enum class DockWidgetFeature : uint8_t {
  none = 0,
  pinnable = 1,
};

inline DockWidgetFeature operator|(DockWidgetFeature a, DockWidgetFeature b) {
  return static_cast<DockWidgetFeature>(static_cast<uint8_t>(a) |
                                        static_cast<uint8_t>(b));
}

inline bool hasDockWidgetFeature(DockWidgetFeature features, DockWidgetFeature flag) {
  return (static_cast<uint8_t>(features) & static_cast<uint8_t>(flag)) != 0;
}

enum class DockAutoHideFlag : uint32_t {
  none = 0,
  feature_enabled = 0x01,
  dock_area_has_pin_button = 0x02,
  show_on_mouse_over = 0x20,
  close_button_collapses = 0x40,
  has_close_button = 0x80,
  has_minimize_button = 0x100,
  open_on_drag_hover = 0x200,
  close_on_outside_click = 0x400,
  default_config = feature_enabled | dock_area_has_pin_button | has_minimize_button |
                   close_on_outside_click,
};

inline DockAutoHideFlag operator|(DockAutoHideFlag a, DockAutoHideFlag b) {
  return static_cast<DockAutoHideFlag>(static_cast<uint32_t>(a) |
                                         static_cast<uint32_t>(b));
}

inline bool testAutoHideFlag(DockAutoHideFlag config, DockAutoHideFlag flag) {
  return (static_cast<uint32_t>(config) & static_cast<uint32_t>(flag)) != 0;
}

struct DockRestoreHint {
  DockId container_id{k_invalid_dock_id};
  DockSlot slot{DockSlot::none};
  int tab_index{0};
  bool valid{false};
};

struct DockAutoHideEntry {
  DockId widget_id{k_invalid_dock_id};
  std::shared_ptr<DockWidget> widget;
  DockEdge edge{DockEdge::left};
  bool expanded{false};
  float expanded_span{280.0f};
  DockRestoreHint restore{};
};

struct DockAutoHideInsets {
  float left{0.0f};
  float right{0.0f};
  float top{0.0f};
  float bottom{0.0f};
};

bool isHorizontalEdge(DockEdge edge);
bool isVerticalEdge(DockEdge edge);
DockSlot edgeToDockSlot(DockEdge edge);
DockEdge defaultAutoHideEdgeForPanel(DockPanelKind panel_kind);

DockAutoHideInsets computeAutoHideInsets(const DockRect& host, float strip_thickness,
                                         int left_count, int right_count, int top_count,
                                         int bottom_count);
DockRect insetHostRect(const DockRect& host, const DockAutoHideInsets& insets);

DockRect computeAutoHideTabRect(DockEdge edge, int index, int count, const DockRect& host,
                                float strip_thickness, float tab_span);
DockRect computeAutoHideOverlayContentRect(DockEdge edge, const DockRect& host,
                                           float strip_thickness, float expanded_span);
DockRect computeAutoHideOverlayTitleRect(const DockRect& content_frame, float title_height);
DockRect computeAutoHideResizeHandleRect(DockEdge edge, const DockRect& content_frame,
                                         float handle_thickness);
float clampAutoHideExpandedSpan(DockEdge edge, float requested, const DockRect& inset_host,
                                float strip_thickness, float min_span, float max_fraction);

// Outer auto-hide band depth: preview_size (32) when strip empty, else max(preview, strip).
std::optional<DockEdge> computeAutoHideDropEdge(const DockRect& host, const glm::vec2& pointer,
                                                float preview_size, float strip_thickness,
                                                int left_tab_count, int right_tab_count,
                                                int top_tab_count, int bottom_tab_count);

// ADS: AutoHideAreaWidth = 32 when strip empty; else strip thickness
DockRect computeAutoHideDropPreviewRect(DockEdge edge, const DockRect& host, float preview_size,
                                        float strip_thickness, int count_on_edge);

struct DockAutoHideGuideLayout {
  DockEdge edge{DockEdge::left};
  DockRect rect{};
};

void computeAutoHideGuideLayouts(const DockRect& host, float icon_width, float icon_height,
                                 float arrow_size, float arrow_gap,
                                 eastl::vector<DockAutoHideGuideLayout>& out_layouts);

std::optional<DockEdge> hitAutoHideGuideEdge(const DockRect& host, float icon_width,
                                             float icon_height, float arrow_size, float arrow_gap,
                                             const glm::vec2& pointer);

}  // namespace Blunder
