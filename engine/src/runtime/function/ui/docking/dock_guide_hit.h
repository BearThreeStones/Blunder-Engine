#pragma once

#include <optional>

#include "EASTL/vector.h"

#include "runtime/function/ui/docking/dock_types.h"

namespace Blunder {

struct DockHostGuideLayout {
  DockSlot slot{DockSlot::none};
  DockRect rect{};
};

bool isInsideOuterMargin(const DockRect& host, const glm::vec2& pointer, float margin);

bool isWithinHostGuideProximity(const DockRect& host, const glm::vec2& pointer,
                                float proximity);

void computeHostGuideLayouts(const DockRect& host, float icon_width, float icon_height,
                             float arrow_size, float arrow_gap,
                             eastl::vector<DockHostGuideLayout>& out_layouts);

std::optional<DockSlot> hitHostGuideSlot(const DockRect& host, float icon_width,
                                         float icon_height, float arrow_size, float arrow_gap,
                                         const glm::vec2& pointer);

DockRect previewRectForHostSlot(const DockRect& host, DockSlot slot, float fraction);

void computeCrossGuideLayouts(const DockRect& content_rect, float guide_size, float guide_gap,
                              eastl::vector<DockHostGuideLayout>& out_layouts);

std::optional<DockSlot> hitCrossGuideSlot(const DockRect& content_rect, float guide_size,
                                          float guide_gap, const glm::vec2& pointer);

}  // namespace Blunder
