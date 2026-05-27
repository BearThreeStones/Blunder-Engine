#pragma once

#include "runtime/function/render/overlay/overlay_base.h"

namespace Blunder {

/// Overlay for drawing selected object wireframes.
/// TODO: Implement wireframe rendering with edge detection.
class WireframeOverlay final : public Overlay {
 public:
  void begin_sync(OverlayResources& res, const OverlayState& state) override;
  void draw_line(VkCommandBuffer cmd, const OverlayState& state) override;
};

}  // namespace Blunder
