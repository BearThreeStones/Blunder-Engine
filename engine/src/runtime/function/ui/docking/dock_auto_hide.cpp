#include "runtime/function/ui/docking/dock_auto_hide.h"

#include <algorithm>

namespace Blunder {

bool isHorizontalEdge(DockEdge edge) {
  return edge == DockEdge::top || edge == DockEdge::bottom;
}

bool isVerticalEdge(DockEdge edge) {
  return edge == DockEdge::left || edge == DockEdge::right;
}

DockSlot edgeToDockSlot(DockEdge edge) {
  switch (edge) {
    case DockEdge::left:
      return DockSlot::left;
    case DockEdge::right:
      return DockSlot::right;
    case DockEdge::top:
      return DockSlot::top;
    case DockEdge::bottom:
      return DockSlot::bottom;
  }
  return DockSlot::right;
}

DockEdge defaultAutoHideEdgeForPanel(DockPanelKind panel_kind) {
  switch (panel_kind) {
    case DockPanelKind::hierarchy:
      return DockEdge::left;
    case DockPanelKind::inspector:
      return DockEdge::right;
    case DockPanelKind::content_browser:
      return DockEdge::bottom;
    default:
      return DockEdge::right;
  }
}

DockAutoHideInsets computeAutoHideInsets(const DockRect& host, float strip_thickness,
                                         int left_count, int right_count, int top_count,
                                         int bottom_count) {
  (void)host;
  DockAutoHideInsets insets{};
  if (left_count > 0) {
    insets.left = strip_thickness;
  }
  if (right_count > 0) {
    insets.right = strip_thickness;
  }
  if (top_count > 0) {
    insets.top = strip_thickness;
  }
  if (bottom_count > 0) {
    insets.bottom = strip_thickness;
  }
  return insets;
}

DockRect insetHostRect(const DockRect& host, const DockAutoHideInsets& insets) {
  const float width = std::max(0.0f, host.width - insets.left - insets.right);
  const float height = std::max(0.0f, host.height - insets.top - insets.bottom);
  return makeDockRect(host.x + insets.left, host.y + insets.top, width, height);
}

DockRect computeAutoHideTabRect(DockEdge edge, int index, int count, const DockRect& host,
                                float strip_thickness, float tab_span) {
  const int safe_count = std::max(1, count);
  const float span = std::max(24.0f, tab_span);
  switch (edge) {
    case DockEdge::left:
      return makeDockRect(0.0f, static_cast<float>(index) * span, strip_thickness,
                          std::min(span, host.height / static_cast<float>(safe_count)));
    case DockEdge::right:
      return makeDockRect(host.width - strip_thickness, static_cast<float>(index) * span,
                          strip_thickness,
                          std::min(span, host.height / static_cast<float>(safe_count)));
    case DockEdge::top:
      return makeDockRect(static_cast<float>(index) * span, 0.0f,
                          std::min(span, host.width / static_cast<float>(safe_count)),
                          strip_thickness);
    case DockEdge::bottom:
      return makeDockRect(static_cast<float>(index) * span, host.height - strip_thickness,
                          std::min(span, host.width / static_cast<float>(safe_count)),
                          strip_thickness);
  }
  return {};
}

DockRect computeAutoHideOverlayContentRect(DockEdge edge, const DockRect& host,
                                           float strip_thickness, float expanded_span) {
  const float span = std::max(64.0f, expanded_span);
  switch (edge) {
    case DockEdge::left:
      return makeDockRect(strip_thickness, 0.0f, span, host.height);
    case DockEdge::right:
      return makeDockRect(host.width - strip_thickness - span, 0.0f, span, host.height);
    case DockEdge::top:
      return makeDockRect(0.0f, strip_thickness, host.width, span);
    case DockEdge::bottom:
      return makeDockRect(0.0f, host.height - strip_thickness - span, host.width, span);
  }
  return {};
}

DockRect computeAutoHideOverlayTitleRect(const DockRect& content_frame, float title_height) {
  const float height = std::min(title_height, content_frame.height);
  return makeDockRect(content_frame.x, content_frame.y, content_frame.width, height);
}

DockRect computeAutoHideResizeHandleRect(DockEdge edge, const DockRect& content_frame,
                                         float handle_thickness) {
  const float handle = std::max(4.0f, handle_thickness);
  switch (edge) {
    case DockEdge::left:
      return makeDockRect(content_frame.right() - handle, content_frame.y, handle,
                          content_frame.height);
    case DockEdge::right:
      return makeDockRect(content_frame.x, content_frame.y, handle, content_frame.height);
    case DockEdge::top:
      return makeDockRect(content_frame.x, content_frame.bottom() - handle, content_frame.width,
                          handle);
    case DockEdge::bottom:
      return makeDockRect(content_frame.x, content_frame.y, content_frame.width, handle);
  }
  return {};
}

float clampAutoHideExpandedSpan(DockEdge edge, float requested, const DockRect& inset_host,
                                float strip_thickness, float min_span, float max_fraction) {
  const float min_size = std::max(64.0f, min_span);
  float max_size = 0.0f;
  if (isVerticalEdge(edge)) {
    max_size = std::max(min_size, inset_host.width * max_fraction);
  } else {
    max_size = std::max(min_size, inset_host.height * max_fraction);
  }
  return std::clamp(requested, min_size, max_size);
}

std::optional<DockEdge> computeAutoHideDropEdge(const DockRect& host, const glm::vec2& pointer,
                                                float preview_size, float strip_thickness,
                                                int left_tab_count, int right_tab_count,
                                                int top_tab_count, int bottom_tab_count) {
  if (!host.valid() || !host.contains(pointer)) {
    return std::nullopt;
  }
  const float local_x = pointer.x - host.x;
  const float local_y = pointer.y - host.y;
  const float left_zone =
      left_tab_count > 0 ? std::max(preview_size, strip_thickness) : preview_size;
  const float right_zone =
      right_tab_count > 0 ? std::max(preview_size, strip_thickness) : preview_size;
  const float top_zone = top_tab_count > 0 ? std::max(preview_size, strip_thickness) : preview_size;
  const float bottom_zone =
      bottom_tab_count > 0 ? std::max(preview_size, strip_thickness) : preview_size;

  if (local_x < left_zone) {
    return DockEdge::left;
  }
  if (local_x > host.width - right_zone) {
    return DockEdge::right;
  }
  if (local_y < top_zone) {
    return DockEdge::top;
  }
  if (local_y > host.height - bottom_zone) {
    return DockEdge::bottom;
  }
  return std::nullopt;
}

DockRect computeAutoHideDropPreviewRect(DockEdge edge, const DockRect& host, float preview_size,
                                        float strip_thickness, int count_on_edge) {
  const float span = count_on_edge > 0 ? strip_thickness : preview_size;
  switch (edge) {
    case DockEdge::left:
      return makeDockRect(host.x, host.y, span, host.height);
    case DockEdge::right:
      return makeDockRect(host.right() - span, host.y, span, host.height);
    case DockEdge::top:
      return makeDockRect(host.x, host.y, host.width, span);
    case DockEdge::bottom:
      return makeDockRect(host.x, host.bottom() - span, host.width, span);
  }
  return {};
}

void computeAutoHideGuideLayouts(const DockRect& host, float icon_width, float icon_height,
                                 float arrow_size, float arrow_gap,
                                 eastl::vector<DockAutoHideGuideLayout>& out_layouts) {
  out_layouts.clear();
  if (!host.valid()) {
    return;
  }
  const float landscape_icon_w = icon_width;
  const float landscape_icon_h = icon_height;
  const float portrait_icon_w = icon_height;
  const float portrait_icon_h = icon_width;
  const float horizontal_total_w = landscape_icon_w;
  const float horizontal_total_h = arrow_size + arrow_gap + landscape_icon_h;
  const float vertical_total_w = arrow_size + arrow_gap + portrait_icon_w;
  const float vertical_total_h = portrait_icon_h;
  const float mid_x = host.x + host.width * 0.5f;
  const float mid_y = host.y + host.height * 0.5f;
  constexpr float k_edge_margin = 2.0f;
  const bool show_horizontal = host.width >= landscape_icon_w * 2.0f;
  const bool show_vertical = host.height >= portrait_icon_h * 2.0f;

  auto push_guide = [&](DockEdge edge, const DockRect& rect) {
    DockAutoHideGuideLayout layout;
    layout.edge = edge;
    layout.rect = rect;
    out_layouts.push_back(layout);
  };

  if (show_vertical) {
    push_guide(DockEdge::left,
               makeDockRect(host.x + k_edge_margin, mid_y - vertical_total_h * 0.5f, vertical_total_w,
                            vertical_total_h));
    push_guide(DockEdge::right,
               makeDockRect(host.right() - k_edge_margin - vertical_total_w,
                            mid_y - vertical_total_h * 0.5f, vertical_total_w, vertical_total_h));
  }
  if (show_horizontal) {
    push_guide(DockEdge::top, makeDockRect(mid_x - horizontal_total_w * 0.5f, host.y + k_edge_margin,
                                           horizontal_total_w, horizontal_total_h));
    push_guide(DockEdge::bottom,
               makeDockRect(mid_x - horizontal_total_w * 0.5f,
                            host.bottom() - k_edge_margin - horizontal_total_h, horizontal_total_w,
                            horizontal_total_h));
  }
}

std::optional<DockEdge> hitAutoHideGuideEdge(const DockRect& host, float icon_width,
                                             float icon_height, float arrow_size, float arrow_gap,
                                             const glm::vec2& pointer) {
  if (!host.valid()) {
    return std::nullopt;
  }
  const float landscape_icon_w = icon_width;
  const float landscape_icon_h = icon_height;
  const float portrait_icon_w = icon_height;
  const float portrait_icon_h = icon_width;
  const float horizontal_total_h = arrow_size + arrow_gap + landscape_icon_h;
  const float vertical_total_w = arrow_size + arrow_gap + portrait_icon_w;
  constexpr float k_edge_margin = 2.0f;
  const bool show_horizontal = host.width >= landscape_icon_w * 2.0f;
  const bool show_vertical = host.height >= portrait_icon_h * 2.0f;

  if (show_vertical) {
    const DockRect left_band =
        makeDockRect(host.x + k_edge_margin, host.y, vertical_total_w, host.height);
    if (left_band.contains(pointer)) {
      return DockEdge::left;
    }
    const DockRect right_band = makeDockRect(host.right() - k_edge_margin - vertical_total_w,
                                             host.y, vertical_total_w, host.height);
    if (right_band.contains(pointer)) {
      return DockEdge::right;
    }
  }
  if (show_horizontal) {
    const DockRect top_band =
        makeDockRect(host.x, host.y + k_edge_margin, host.width, horizontal_total_h);
    if (top_band.contains(pointer)) {
      return DockEdge::top;
    }
    const DockRect bottom_band = makeDockRect(host.x, host.bottom() - k_edge_margin - horizontal_total_h,
                                              host.width, horizontal_total_h);
    if (bottom_band.contains(pointer)) {
      return DockEdge::bottom;
    }
  }
  return std::nullopt;
}

}  // namespace Blunder
