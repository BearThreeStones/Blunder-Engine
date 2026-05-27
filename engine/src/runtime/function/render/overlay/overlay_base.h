#pragma once

#include <vulkan/vulkan.h>

namespace Blunder {

struct OverlayState;
struct OverlayResources;

/// Base interface for all overlay types (Grid, Axes, Wireframe, etc.).
/// Mirrors Blender's overlay::Overlay lifecycle.
struct Overlay {
  bool enabled_ = false;

  virtual ~Overlay() = default;

  /// Create/reset internal state. Called once per frame before drawing.
  virtual void begin_sync(OverlayResources& res, const OverlayState& state) = 0;

  /// Draw line-topology overlays (grid, wireframe, bones, etc.).
  virtual void draw_line(VkCommandBuffer cmd, const OverlayState& state) {}

  /// Draw solid/filled overlays.
  virtual void draw(VkCommandBuffer cmd, const OverlayState& state) {}

  /// Draw without depth test (origins, motion paths, etc.).
  virtual void draw_color_only(VkCommandBuffer cmd, const OverlayState& state) {}
};

}  // namespace Blunder
