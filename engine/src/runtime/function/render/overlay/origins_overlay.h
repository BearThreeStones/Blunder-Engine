#pragma once

#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

/// Overlay for drawing object origin points.
/// TODO: Implement origin dot rendering.
class OriginsOverlay final : public Overlay {
 public:
  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_color_only(VkCommandBuffer cmd, const OverlayState& state) override;
};

}  // namespace Blunder
