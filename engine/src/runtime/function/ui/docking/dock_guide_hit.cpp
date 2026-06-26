#include "runtime/function/ui/docking/dock_guide_hit.h"

#include "runtime/function/ui/docking/dock_auto_hide.h"

#include <algorithm>

namespace Blunder {

bool isInsideOuterMargin(const DockRect& host, const glm::vec2& pointer, float margin) {
  if (!host.valid() || margin <= 0.0f) {
    return false;
  }
  const float local_x = pointer.x - host.x;
  const float local_y = pointer.y - host.y;
  return local_x < margin || local_y < margin || local_x > host.width - margin ||
         local_y > host.height - margin;
}

bool isWithinHostGuideProximity(const DockRect& host, const glm::vec2& pointer,
                                float proximity) {
  if (!host.valid() || !host.contains(pointer) || proximity <= 0.0f) {
    return false;
  }
  const float local_x = pointer.x - host.x;
  const float local_y = pointer.y - host.y;
  const float dist_left = local_x;
  const float dist_right = host.width - local_x;
  const float dist_top = local_y;
  const float dist_bottom = host.height - local_y;
  const float min_dist =
      std::min({dist_left, dist_right, dist_top, dist_bottom});
  return min_dist <= proximity;
}

namespace {

DockSlot slotForEdge(DockEdge edge) {
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
  return DockSlot::none;
}

}  // namespace

void computeHostGuideLayouts(const DockRect& host, float icon_width, float icon_height,
                             float arrow_size, float arrow_gap,
                             eastl::vector<DockHostGuideLayout>& out_layouts) {
  out_layouts.clear();
  eastl::vector<DockAutoHideGuideLayout> edge_layouts;
  computeAutoHideGuideLayouts(host, icon_width, icon_height, arrow_size, arrow_gap,
                              edge_layouts);
  for (const DockAutoHideGuideLayout& layout : edge_layouts) {
    DockHostGuideLayout host_layout;
    host_layout.slot = slotForEdge(layout.edge);
    host_layout.rect = layout.rect;
    out_layouts.push_back(host_layout);
  }
}

std::optional<DockSlot> hitHostGuideSlot(const DockRect& host, float icon_width,
                                         float icon_height, float arrow_size, float arrow_gap,
                                         const glm::vec2& pointer) {
  eastl::vector<DockHostGuideLayout> layouts;
  computeHostGuideLayouts(host, icon_width, icon_height, arrow_size, arrow_gap, layouts);
  for (const DockHostGuideLayout& layout : layouts) {
    if (layout.rect.contains(pointer)) {
      return layout.slot;
    }
  }
  return std::nullopt;
}

DockRect previewRectForHostSlot(const DockRect& host, DockSlot slot, float fraction) {
  if (!host.valid()) {
    return {};
  }
  const float frac = std::clamp(fraction, 0.25f, 0.75f);
  const float half_w = host.width * frac;
  const float half_h = host.height * frac;
  switch (slot) {
    case DockSlot::left:
      return makeDockRect(host.x, host.y, half_w, host.height);
    case DockSlot::right:
      return makeDockRect(host.right() - half_w, host.y, half_w, host.height);
    case DockSlot::top:
      return makeDockRect(host.x, host.y, host.width, half_h);
    case DockSlot::bottom:
      return makeDockRect(host.x, host.bottom() - half_h, host.width, half_h);
    default:
      return {};
  }
}

void computeCrossGuideLayouts(const DockRect& content_rect, float guide_size, float guide_gap,
                              eastl::vector<DockHostGuideLayout>& out_layouts) {
  out_layouts.clear();
  if (!content_rect.valid()) {
    return;
  }
  const glm::vec2 center = content_rect.center();
  const float size = std::max(24.0f, guide_size);
  const float half = size * 0.5f;
  const float step = size + guide_gap;
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
    DockHostGuideLayout layout;
    layout.slot = anchor.slot;
    layout.rect = makeDockRect(anchor.cx - half, anchor.cy - half, size, size);
    out_layouts.push_back(layout);
  }
}

std::optional<DockSlot> hitCrossGuideSlot(const DockRect& content_rect, float guide_size,
                                          float guide_gap, const glm::vec2& pointer) {
  eastl::vector<DockHostGuideLayout> layouts;
  computeCrossGuideLayouts(content_rect, guide_size, guide_gap, layouts);
  for (const DockHostGuideLayout& layout : layouts) {
    if (layout.rect.contains(pointer)) {
      return layout.slot;
    }
  }
  return std::nullopt;
}

}  // namespace Blunder
