#pragma once

#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

/// Overlay for drawing origin axis gizmos.
/// TODO: Extract axis line rendering from grid shader into this overlay.
class AxesOverlay final : public Overlay {
 public:
  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_line(VkCommandBuffer cmd, const OverlayState& state) override;
};

}  // namespace Blunder
